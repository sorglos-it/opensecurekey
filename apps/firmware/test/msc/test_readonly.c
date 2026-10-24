/* Der eigentliche Zweck des Pruefsatzes: **das Laufwerk nimmt nichts an.**
 *
 * Geprueft wird nicht, dass WRITE scheitert - das waere zu wenig. Geprueft
 * wird jeder Befehl, der auf einem anderen Weg etwas veraendern koennte:
 * MODE SELECT stellt das Geraet um, FORMAT UNIT loescht, WRITE BUFFER
 * schreibt in die Firmware, UNMAP und SANITIZE verwerfen Bloecke. Alle enden
 * gleich: CSW FEHLER, Sinnesdaten DATA PROTECT / WRITE PROTECTED, und der
 * Flash wird nicht einmal angefasst.
 *
 * Dazu der Fall, den ein Angreifer wirklich faehrt: Befehl schicken, Daten
 * hinterherschicken, als waere er angenommen worden. Auch dann darf nichts
 * ankommen. */
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
	osk_msc_write_attempts_reset();
	fake_drive_reads_reset();
}

static uint8_t last_status(void)
{
	uint8_t status = 0xfe;

	mock_sink_csw(NULL, NULL, &status);
	return status;
}

static void fetch_sense(uint8_t *key, uint8_t *asc)
{
	const uint8_t cdb[6] = {0x03, 0, 0, 0, 18, 0};

	mock_sink_reset();
	mock_cbw(0x11111111u, 18, true, 0, cdb, 6);

	const size_t total = mock_sink_len();
	const uint8_t *p = mock_sink_data();

	*key = (total >= 18u + OSK_MSC_CSW_LEN) ? (uint8_t)(p[2] & 0x0fu) : 0xffu;
	*asc = (total >= 18u + OSK_MSC_CSW_LEN) ? p[12] : 0xffu;
}

/* Jeder Befehl, der auf diesem Laufwerk nichts zu suchen hat. Die Laenge
 * steht dabei, weil ein zu kurzer Befehlsblock schon vorher abgewiesen
 * wuerde und der Fall dann die falsche Wand messen wuerde. */
typedef struct {
	const char *name;
	uint8_t op;
	uint8_t cdb_len;
	uint32_t host_len; /* wie viel der Rechner schicken will */
} forbidden_t;

static const forbidden_t FORBIDDEN[] = {
    {"FORMAT UNIT", 0x04, 6, 0},
    {"REASSIGN BLOCKS", 0x07, 6, 4},
    {"WRITE(6)", 0x0a, 6, OSK_MSC_BLOCK_LEN},
    {"MODE SELECT(6)", 0x15, 6, 12},
    {"ERASE(6)", 0x19, 6, 0},
    {"WRITE(10)", 0x2a, 10, OSK_MSC_BLOCK_LEN},
    {"ERASE(10)", 0x2c, 10, 0},
    {"WRITE AND VERIFY(10)", 0x2e, 10, OSK_MSC_BLOCK_LEN},
    {"WRITE BUFFER", 0x3b, 10, 64},
    {"WRITE LONG(10)", 0x3f, 10, 520},
    {"WRITE SAME(10)", 0x41, 10, OSK_MSC_BLOCK_LEN},
    {"UNMAP", 0x42, 10, 24},
    {"SANITIZE", 0x48, 10, 8},
    {"LOG SELECT", 0x4c, 10, 4},
    {"MODE SELECT(10)", 0x55, 10, 16},
    {"PERSISTENT RESERVE OUT", 0x5e, 10, 24},
    {"ATA PASS-THROUGH(16)", 0x85, 16, OSK_MSC_BLOCK_LEN},
    {"COMPARE AND WRITE", 0x89, 16, 2u * OSK_MSC_BLOCK_LEN},
    {"WRITE(16)", 0x8a, 16, OSK_MSC_BLOCK_LEN},
    {"WRITE AND VERIFY(16)", 0x8e, 16, OSK_MSC_BLOCK_LEN},
    {"WRITE SAME(16)", 0x93, 16, OSK_MSC_BLOCK_LEN},
    {"ATA PASS-THROUGH(12)", 0xa1, 12, OSK_MSC_BLOCK_LEN},
    {"WRITE(12)", 0xaa, 12, OSK_MSC_BLOCK_LEN},
    {"ERASE(12)", 0xac, 12, 0},
    {"WRITE AND VERIFY(12)", 0xae, 12, OSK_MSC_BLOCK_LEN},
};

#define FORBIDDEN_COUNT (sizeof(FORBIDDEN) / sizeof(FORBIDDEN[0]))

/* Schickt den Befehl und, wenn er eine Datenphase hat, auch die Daten -
 * genau so, wie ein Rechner es tut, der mit einem Ja rechnet. */
static void send_forbidden(const forbidden_t *f, uint32_t tag)
{
	uint8_t cdb[OSK_MSC_CDB_MAX];
	uint8_t chunk[OSK_MSC_BLOCK_LEN];

	memset(cdb, 0, sizeof(cdb));
	cdb[0] = f->op;
	/* Eine Adresse mitten im Laufwerk, damit der Befehl nicht schon an
	 * einer Bereichspruefung haengenbleibt. */
	cdb[5] = 8;

	mock_sink_reset();
	mock_cbw(tag, f->host_len, false, 0, cdb, f->cdb_len);

	uint32_t left = f->host_len;

	memset(chunk, 0xa5, sizeof(chunk));
	while (left > 0u) {
		const uint32_t take = (left > sizeof(chunk)) ? (uint32_t)sizeof(chunk) : left;

		osk_msc_rx(chunk, take);
		left -= take;
	}
}

static void case_every_write_is_refused(void)
{
	setup();

	for (size_t i = 0; i < FORBIDDEN_COUNT; i++) {
		const forbidden_t *f = &FORBIDDEN[i];
		uint8_t key = 0, asc = 0;

		send_forbidden(f, (uint32_t)(0x1000u + i));

		T_TRUE(f->name, last_status() == OSK_MSC_CSW_FAILED);
		/* Es gibt keine Datenphase zum Rechner hin - nur das CSW. */
		T_EQ("nur das CSW kam zurueck", mock_sink_len(), OSK_MSC_CSW_LEN);

		fetch_sense(&key, &asc);
		T_EQ("Grund ist Schreibschutz", key, OSK_MSC_SK_DATA_PROTECT);
		T_EQ("und zwar genau der", asc, OSK_MSC_ASC_WRITE_PROTECTED);
	}

	T_EQ("jeder Versuch ist gezaehlt", osk_msc_write_attempts(), FORBIDDEN_COUNT);
	T_EQ("kein Block wurde gelesen", fake_drive_reads(), 0);
	T_TRUE("und erst recht keiner geschrieben", !fake_drive_was_written());
}

/* Nach einem abgewiesenen Schreibbefehl mit Datenphase darf das CSW erst
 * kommen, wenn die angekuendigten Daten durch sind - sonst haengt der
 * Rechner an seiner halben Uebertragung. */
static void case_data_phase_is_swallowed(void)
{
	uint8_t cdb[10];
	uint8_t chunk[OSK_MSC_BLOCK_LEN];
	uint32_t residue = 0;

	setup();
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x2a; /* WRITE(10) */
	cdb[8] = 2;    /* zwei Bloecke */
	mock_cbw(0x77u, 2u * OSK_MSC_BLOCK_LEN, false, 0, cdb, 10);
	T_EQ("noch kein CSW", mock_sink_calls(), 0);

	memset(chunk, 0x5a, sizeof(chunk));
	osk_msc_rx(chunk, sizeof(chunk));
	T_EQ("nach dem ersten Block immer noch keins", mock_sink_calls(), 0);

	osk_msc_rx(chunk, sizeof(chunk));
	T_EQ("jetzt genau eins", mock_sink_calls(), 1);
	T_EQ("und es meldet Fehler", last_status(), OSK_MSC_CSW_FAILED);
	mock_sink_csw(NULL, &residue, NULL);
	T_EQ("nichts wurde uebernommen", residue, 2u * OSK_MSC_BLOCK_LEN);
	T_TRUE("der Flash hat nichts gesehen", !fake_drive_was_written());

	/* Danach geht es ganz normal weiter. */
	const uint8_t ok[6] = {0x00, 0, 0, 0, 0, 0};

	mock_sink_reset();
	mock_cbw(0x78u, 0, false, 0, ok, 6);
	T_EQ("der naechste Befehl laeuft", last_status(), OSK_MSC_CSW_PASSED);
}

/* Der Schreibschutz steht auch dort, wo ein Betriebssystem zuerst
 * nachsieht - sonst formt es den Schreibbefehl erst gar nicht und meldet
 * dem Nutzer etwas Falsches. */
static void case_write_protect_bit(void)
{
	const uint8_t ms6[6] = {0x1a, 0, 0x3f, 0, 192, 0};
	const uint8_t ms10[10] = {0x5a, 0, 0x3f, 0, 0, 0, 0, 0, 192, 0};

	setup();
	mock_cbw(1, 192, true, 0, ms6, 6);
	T_EQ("MODE SENSE(6) gut", last_status(), OSK_MSC_CSW_PASSED);
	T_TRUE("Schreibschutzbit gesetzt", (mock_sink_data()[2] & 0x80u) != 0u);
	T_EQ("keine Blockbeschreiber", mock_sink_data()[3], 0);

	mock_sink_reset();
	mock_cbw(2, 192, true, 0, ms10, 10);
	T_EQ("MODE SENSE(10) gut", last_status(), OSK_MSC_CSW_PASSED);
	T_TRUE("auch hier gesetzt", (mock_sink_data()[3] & 0x80u) != 0u);

	/* Eine unbekannte Seite wird abgewiesen, nicht erfunden. */
	const uint8_t weird[6] = {0x1a, 0, 0x2a, 0, 192, 0};

	mock_sink_reset();
	mock_cbw(3, 192, true, 0, weird, 6);
	T_EQ("unbekannte Seite abgewiesen", last_status(), OSK_MSC_CSW_FAILED);
}

/* VERIFY ohne Vergleichsdaten ist harmlos; VERIFY mit BYTCHK waere eine
 * Datenphase vom Rechner herein, und die gibt es hier nicht. */
static void case_verify(void)
{
	uint8_t cdb[10];

	setup();
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x2f;
	cdb[8] = 1;
	mock_cbw(1, 0, false, 0, cdb, 10);
	T_EQ("VERIFY ohne BYTCHK geht", last_status(), OSK_MSC_CSW_PASSED);

	cdb[1] = 0x02;
	mock_sink_reset();
	mock_cbw(2, 0, false, 0, cdb, 10);
	T_EQ("VERIFY mit BYTCHK abgewiesen", last_status(), OSK_MSC_CSW_FAILED);
	T_TRUE("nichts geschrieben", !fake_drive_was_written());
}

/* Ausgeschaltet nimmt das Laufwerk auch keine Schreibversuche mehr
 * entgegen - und zaehlt sie folgerichtig auch nicht. */
static void case_refused_while_off(void)
{
	uint8_t cdb[10];

	setup();
	osk_msc_set_enabled(false);
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x2a;
	cdb[8] = 1;
	mock_sink_reset();
	mock_cbw(1, OSK_MSC_BLOCK_LEN, false, 0, cdb, 10);
	T_EQ("keine Antwort", mock_sink_calls(), 0);
	T_EQ("nichts gezaehlt", osk_msc_write_attempts(), 0);
	T_TRUE("nichts geschrieben", !fake_drive_was_written());
	osk_msc_set_enabled(true);
}

int main(void)
{
	case_every_write_is_refused();
	case_data_phase_is_swallowed();
	case_write_protect_bit();
	case_verify();
	case_refused_while_off();
	fake_drive_free();
	return t_report("test_readonly");
}
