/* Bulk-Only Transport und der SCSI-Satz des Laufwerks. Siehe usb_msc.h.
 * Spezifikationen: USB Mass Storage Bulk-Only Transport 1.0, SPC-4, SBC-3.
 *
 * Aufbau der Datei:
 *   1. Zustand und kleine Helfer
 *   2. Sinnesdaten (sense)
 *   3. Die Befehle, die lesen duerfen
 *   4. Die Befehle, die abgewiesen werden - und warum einzeln aufgezaehlt
 *   5. Der Rahmen: CBW herein, CSW hinaus
 *
 * Eine Regel zieht sich durch alles: eine Laenge wird geprueft, bevor an der
 * Stelle gelesen wird, auf die sie zeigt. Der Befehlsblock eines CBW ist bis
 * zu 16 Byte lang, aber der Rechner sagt selbst, wie viel davon gilt - und
 * diese Angabe ist eine Behauptung, keine Tatsache. */
#include <string.h>
#include "usb_msc.h"
#include "osk_transport.h"

/* --- 1. Zustand -------------------------------------------------------- */

/* SCSI-Befehle, die lesen duerfen. */
#define SCSI_TEST_UNIT_READY 0x00
#define SCSI_REQUEST_SENSE 0x03
#define SCSI_INQUIRY 0x12
#define SCSI_MODE_SENSE_6 0x1a
#define SCSI_START_STOP_UNIT 0x1b
#define SCSI_PREVENT_ALLOW 0x1e
#define SCSI_READ_FORMAT_CAPACITIES 0x23
#define SCSI_READ_CAPACITY_10 0x25
#define SCSI_READ_6 0x08
#define SCSI_READ_10 0x28
#define SCSI_READ_12 0xa8
#define SCSI_READ_16 0x88
#define SCSI_VERIFY_10 0x2f
#define SCSI_SYNCHRONIZE_CACHE 0x35
#define SCSI_MODE_SENSE_10 0x5a
#define SCSI_SERVICE_ACTION_IN_16 0x9e
#define SCSI_SAI_READ_CAPACITY_16 0x10

/* Laengste Antwort, die ein Befehl ohne Nutzdaten erzeugt. READ CAPACITY(16)
 * will 32 Byte, INQUIRY 36 - ein Block reicht fuer alles mit Abstand. */
#define MSC_REPLY_MAX OSK_MSC_BLOCK_LEN

/* Hoechstzahl Bloecke in einem Lesebefehl. Mehr hat kein Rechner noetig, und
 * eine Obergrenze haelt die Schleife endlich, egal was im Befehl steht. */
#define MSC_READ_BLOCKS_MAX 256u

typedef struct {
	uint8_t key;
	uint8_t asc;
	uint8_t ascq;
} sense_t;

static osk_msc_read_fn g_read;
static osk_msc_send_fn g_send;
static void *g_ctx;
static uint32_t g_blocks;
static bool g_on;
static bool g_stalled;
static sense_t g_sense;
static uint32_t g_write_attempts;

/* Was nach einem abgewiesenen Schreibbefehl noch hereinkommt: der Rechner
 * hat seine Daten schon unterwegs. Sie werden gezaehlt und weggeworfen; erst
 * wenn die angekuendigte Menge durch ist, kommt das CSW. */
static uint32_t g_discard_left;
static uint32_t g_discard_tag;
static uint32_t g_discard_total;

static void put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static uint32_t get_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
	       (uint32_t)p[3];
}

static uint16_t get_be16(const uint8_t *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void put_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void send(const uint8_t *data, size_t len)
{
	if (g_send)
		g_send(data, len, g_ctx);
}

/* --- 2. Sinnesdaten ---------------------------------------------------- */

static void sense_set(uint8_t key, uint8_t asc, uint8_t ascq)
{
	g_sense.key = key;
	g_sense.asc = asc;
	g_sense.ascq = ascq;
}

static void sense_clear(void)
{
	sense_set(OSK_MSC_SK_NO_SENSE, 0, 0);
}

/* --- 3. Die Befehle, die lesen duerfen --------------------------------- */

/* Der Name des Geraets. Absichtlich ohne Seriennummer und ohne irgendetwas,
 * was zwei Sticks unterscheidbar macht: dieses Laufwerk ist an jedem fremden
 * Rechner sichtbar, und eine Kennung darin waere eine Spur, die der Nutzer
 * nie gelegt hat. */
static const char INQ_VENDOR[8] = {'S', 'o', 'r', 'g', 'l', 'o', 's', ' '};
static const char INQ_PRODUCT[16] = {'O', 'S', 'K', ' ', 'P', 'r', 'o', 'g',
				     'r', 'a', 'm', 'm', 'e', ' ', ' ', ' '};
static const char INQ_REVISION[4] = {'0', '0', '0', '1'};

static size_t cmd_inquiry(const uint8_t *cdb, uint8_t cdb_len, uint8_t *out)
{
	if (cdb_len < 6)
		return 0;

	const bool evpd = (cdb[1] & 0x01u) != 0u;
	const uint8_t page = cdb[2];

	if (evpd) {
		switch (page) {
		case 0x00: /* welche Seiten es gibt */
			out[0] = 0x00;
			out[1] = 0x00;
			out[2] = 0x00;
			out[3] = 0x02;
			out[4] = 0x00;
			out[5] = 0x80;
			return 6;
		case 0x80: /* Seriennummer - bewusst fuer alle gleich */
			out[0] = 0x00;
			out[1] = 0x80;
			out[2] = 0x00;
			out[3] = 0x04;
			out[4] = '0';
			out[5] = '0';
			out[6] = '0';
			out[7] = '1';
			return 8;
		default:
			sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
			return 0;
		}
	}

	/* Eine Seitennummer ohne EVPD ist ein Widerspruch. */
	if (page != 0x00) {
		sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
		return 0;
	}

	memset(out, 0, 36);
	out[0] = 0x00; /* Direktzugriff, Blockgeraet */
	out[1] = 0x80; /* Wechselmedium */
	out[2] = 0x06; /* SPC-4 */
	out[3] = 0x02; /* Antwortformat 2 */
	out[4] = 31;   /* zusaetzliche Laenge */
	memcpy(out + 8, INQ_VENDOR, sizeof(INQ_VENDOR));
	memcpy(out + 16, INQ_PRODUCT, sizeof(INQ_PRODUCT));
	memcpy(out + 32, INQ_REVISION, sizeof(INQ_REVISION));
	return 36;
}

static size_t cmd_request_sense(uint8_t *out)
{
	memset(out, 0, 18);
	out[0] = 0x70; /* festes Format, aktueller Fehler */
	out[2] = (uint8_t)(g_sense.key & 0x0fu);
	out[7] = 10; /* zusaetzliche Laenge */
	out[12] = g_sense.asc;
	out[13] = g_sense.ascq;
	/* Gelesen heisst abgeholt: der naechste Befehl faengt sauber an. */
	sense_clear();
	return 18;
}

/* Das WP-Bit sitzt im geraetespezifischen Parameter des Kopfes. Es ist die
 * Stelle, an der ein Betriebssystem nachsieht, bevor es ueberhaupt einen
 * Schreibbefehl formt - darum steht es hier, und zusaetzlich wird jeder
 * Schreibbefehl abgewiesen. Eins von beidem waere zu wenig. */
#define MSC_DEV_SPEC_WP 0x80

static bool mode_page_known(uint8_t page)
{
	return page == 0x3f || page == 0x08 || page == 0x1c || page == 0x00;
}

static size_t cmd_mode_sense_6(const uint8_t *cdb, uint8_t cdb_len, uint8_t *out)
{
	if (cdb_len < 6)
		return 0;
	if (!mode_page_known((uint8_t)(cdb[2] & 0x3fu))) {
		sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
		return 0;
	}
	out[0] = 3; /* Laenge der folgenden Daten */
	out[1] = 0; /* Medientyp */
	out[2] = MSC_DEV_SPEC_WP;
	out[3] = 0; /* keine Blockbeschreiber */
	return 4;
}

static size_t cmd_mode_sense_10(const uint8_t *cdb, uint8_t cdb_len, uint8_t *out)
{
	if (cdb_len < 10)
		return 0;
	if (!mode_page_known((uint8_t)(cdb[2] & 0x3fu))) {
		sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
		return 0;
	}
	memset(out, 0, 8);
	out[0] = 0;
	out[1] = 6;
	out[3] = MSC_DEV_SPEC_WP;
	return 8;
}

static size_t cmd_read_capacity_10(uint8_t *out)
{
	const uint32_t last = (g_blocks > 0u) ? (g_blocks - 1u) : 0u;

	put_be32(out, last);
	put_be32(out + 4, (uint32_t)OSK_MSC_BLOCK_LEN);
	return 8;
}

static size_t cmd_read_capacity_16(const uint8_t *cdb, uint8_t cdb_len, uint8_t *out)
{
	if (cdb_len < 16 || (cdb[1] & 0x1fu) != SCSI_SAI_READ_CAPACITY_16) {
		sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
		return 0;
	}

	const uint32_t last = (g_blocks > 0u) ? (g_blocks - 1u) : 0u;

	memset(out, 0, 32);
	put_be32(out + 4, last); /* die oberen vier Byte bleiben 0 */
	put_be32(out + 8, (uint32_t)OSK_MSC_BLOCK_LEN);
	return 32;
}

static size_t cmd_read_format_capacities(uint8_t *out)
{
	memset(out, 0, 12);
	out[3] = 8; /* Laenge der Liste */
	put_be32(out + 4, g_blocks);
	out[8] = 0x02; /* formatiertes Medium */
	out[9] = 0x00;
	out[10] = (uint8_t)(OSK_MSC_BLOCK_LEN >> 8);
	out[11] = (uint8_t)OSK_MSC_BLOCK_LEN;
	return 12;
}

/* Liest die Adresse und die Blockzahl aus einem Lesebefehl. Gibt false
 * zurueck, wenn der Befehl zu kurz ist oder ausserhalb des Laufwerks zeigt -
 * und setzt dann schon die passenden Sinnesdaten. */
static bool read_range(uint8_t op, const uint8_t *cdb, uint8_t cdb_len, uint32_t *lba_out,
		       uint32_t *count_out)
{
	uint32_t lba = 0;
	uint32_t count = 0;

	switch (op) {
	case SCSI_READ_6:
		if (cdb_len < 6)
			goto bad_field;
		lba =
		    (((uint32_t)cdb[1] & 0x1fu) << 16) | ((uint32_t)cdb[2] << 8) | (uint32_t)cdb[3];
		count = (cdb[4] == 0u) ? 256u : (uint32_t)cdb[4];
		break;
	case SCSI_READ_10:
		if (cdb_len < 10)
			goto bad_field;
		lba = get_be32(cdb + 2);
		count = get_be16(cdb + 7);
		break;
	case SCSI_READ_12:
		if (cdb_len < 12)
			goto bad_field;
		lba = get_be32(cdb + 2);
		count = get_be32(cdb + 6);
		break;
	case SCSI_READ_16:
		if (cdb_len < 16)
			goto bad_field;
		/* Die oberen vier Byte der 64-Bit-Adresse muessen 0 sein; das
		 * Laufwerk ist Megabytes gross, nicht Exabytes. */
		if (get_be32(cdb + 2) != 0u)
			goto out_of_range;
		lba = get_be32(cdb + 6);
		count = get_be32(cdb + 10);
		break;
	default:
		goto bad_field;
	}

	if (count > MSC_READ_BLOCKS_MAX)
		goto bad_field;
	/* Addiert wird erst, nachdem feststeht, dass es nicht ueberlaeuft. */
	if (lba > g_blocks || count > g_blocks - lba)
		goto out_of_range;

	*lba_out = lba;
	*count_out = count;
	return true;

bad_field:
	sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
	return false;
out_of_range:
	sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_LBA_OUT_OF_RANGE, 0x00);
	return false;
}

/* --- 4. Die Befehle, die abgewiesen werden ----------------------------- */

/* Einzeln aufgezaehlt und nicht ueber "alles ausser Lesen" abgehandelt,
 * damit jeder Name hier steht und im Test nachgeschlagen werden kann. Es
 * sind drei Sorten, und alle drei enden gleich:
 *   - die offensichtlichen: WRITE in allen vier Laengen
 *   - die, die wie etwas anderes aussehen: MODE SELECT stellt das Geraet um,
 *     FORMAT UNIT loescht, WRITE BUFFER schreibt in die Firmware
 *   - die neueren: UNMAP, WRITE SAME, SANITIZE, COMPARE AND WRITE
 * Ein Befehl, der hier fehlt, faellt auf INVALID OPERATION CODE - auch das
 * ist eine Abweisung, nur mit einem anderen Grund. */
static bool is_write_command(uint8_t op)
{
	switch (op) {
	case 0x04: /* FORMAT UNIT */
	case 0x07: /* REASSIGN BLOCKS */
	case 0x0a: /* WRITE(6) */
	case 0x15: /* MODE SELECT(6) */
	case 0x19: /* ERASE(6) */
	case 0x2a: /* WRITE(10) */
	case 0x2c: /* ERASE(10) */
	case 0x2e: /* WRITE AND VERIFY(10) */
	case 0x3b: /* WRITE BUFFER */
	case 0x3f: /* WRITE LONG(10) */
	case 0x41: /* WRITE SAME(10) */
	case 0x42: /* UNMAP */
	case 0x48: /* SANITIZE */
	case 0x4c: /* LOG SELECT */
	case 0x55: /* MODE SELECT(10) */
	case 0x5e: /* PERSISTENT RESERVE OUT */
	case 0x89: /* COMPARE AND WRITE */
	case 0x8a: /* WRITE(16) */
	case 0x8e: /* WRITE AND VERIFY(16) */
	case 0x93: /* WRITE SAME(16) */
	case 0xa1: /* ATA PASS-THROUGH(12) */
	case 0x85: /* ATA PASS-THROUGH(16) */
	case 0xaa: /* WRITE(12) */
	case 0xac: /* ERASE(12) */
	case 0xae: /* WRITE AND VERIFY(12) */
		return true;
	default:
		return false;
	}
}

/* --- 5. Der Rahmen ----------------------------------------------------- */

static void send_csw(uint32_t tag, uint32_t residue, uint8_t status)
{
	uint8_t csw[OSK_MSC_CSW_LEN];

	put_le32(csw + 0, OSK_MSC_CSW_SIG);
	put_le32(csw + 4, tag);
	put_le32(csw + 8, residue);
	csw[12] = status;
	send(csw, sizeof(csw));
}

/* Schickt die Bloecke eines Lesebefehls einzeln hinaus. Bricht ein Block weg,
 * endet die Uebertragung dort und das CSW meldet den Rest als Rueckstand -
 * halbe Daten als "alles gut" zu melden waere die schlimmere Antwort. */
static uint32_t send_blocks(uint32_t lba, uint32_t count)
{
	uint8_t block[OSK_MSC_BLOCK_LEN];
	uint32_t done = 0;

	for (uint32_t i = 0; i < count; i++) {
		if (!g_read || g_read(lba + i, block, g_ctx) != OSK_OK) {
			sense_set(OSK_MSC_SK_NOT_READY, OSK_MSC_ASC_MEDIUM_NOT_PRESENT, 0x00);
			break;
		}
		send(block, sizeof(block));
		done++;
	}
	memset(block, 0, sizeof(block));
	return done;
}

static void handle_cbw(const uint8_t *cbw)
{
	uint8_t reply[MSC_REPLY_MAX];
	const uint32_t tag = get_le32(cbw + 4);
	const uint32_t host_len = get_le32(cbw + 8);
	const bool host_wants_in = (cbw[12] & 0x80u) != 0u;
	const uint8_t lun = (uint8_t)(cbw[13] & 0x0fu);
	const uint8_t cdb_len = (uint8_t)(cbw[14] & 0x1fu);
	const uint8_t *cdb = cbw + 15;

	/* Ein Befehlsblock ohne Laenge oder laenger als das Feld ist kein
	 * Befehl. Hier wird abgewiesen, bevor cdb[0] ueberhaupt gelesen wird. */
	if (cdb_len == 0u || cdb_len > OSK_MSC_CDB_MAX) {
		sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
		send_csw(tag, host_len, OSK_MSC_CSW_FAILED);
		return;
	}
	/* Es gibt genau eine Einheit. */
	if (lun != 0u) {
		sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
		send_csw(tag, host_len, OSK_MSC_CSW_FAILED);
		return;
	}

	const uint8_t op = cdb[0];

	/* Der wichtigste Zweig der Datei. Vor allem anderen, damit kein
	 * spaeterer Fall ihn je umgehen kann. */
	if (is_write_command(op)) {
		g_write_attempts++;
		sense_set(OSK_MSC_SK_DATA_PROTECT, OSK_MSC_ASC_WRITE_PROTECTED, 0x00);
		if (host_len > 0u && !host_wants_in) {
			/* Der Rechner schickt gleich Daten. Sie werden gezaehlt
			 * und weggeworfen, das CSW kommt danach. */
			g_discard_left = host_len;
			g_discard_total = host_len;
			g_discard_tag = tag;
			return;
		}
		send_csw(tag, host_len, OSK_MSC_CSW_FAILED);
		return;
	}

	/* Ab hier wird nur gelesen. Ein Befehl, der Daten vom Rechner haben
	 * will, passt nicht mehr dazu: Phasenfehler. */
	if (host_len > 0u && !host_wants_in) {
		sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
		send_csw(tag, host_len, OSK_MSC_CSW_PHASE);
		return;
	}

	size_t produced = 0;
	bool ok = true;

	switch (op) {
	case SCSI_TEST_UNIT_READY:
	case SCSI_START_STOP_UNIT:
	case SCSI_PREVENT_ALLOW:
	case SCSI_SYNCHRONIZE_CACHE:
		/* Nichts zu tun und nichts zu melden: es gibt keinen
		 * Zwischenspeicher, und auswerfen laesst sich nichts. */
		break;
	case SCSI_VERIFY_10:
		if (cdb_len < 10) {
			sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
			ok = false;
			break;
		}
		/* BYTCHK verlangt einen Vergleich mit Daten des Rechners - das
		 * waere eine Datenphase vom Rechner herein, und die gibt es
		 * hier nicht. */
		if ((cdb[1] & 0x02u) != 0u) {
			sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
			ok = false;
		}
		break;
	case SCSI_REQUEST_SENSE:
		produced = cmd_request_sense(reply);
		break;
	case SCSI_INQUIRY:
		produced = cmd_inquiry(cdb, cdb_len, reply);
		ok = produced > 0;
		break;
	case SCSI_MODE_SENSE_6:
		produced = cmd_mode_sense_6(cdb, cdb_len, reply);
		ok = produced > 0;
		break;
	case SCSI_MODE_SENSE_10:
		produced = cmd_mode_sense_10(cdb, cdb_len, reply);
		ok = produced > 0;
		break;
	case SCSI_READ_CAPACITY_10:
		produced = cmd_read_capacity_10(reply);
		break;
	case SCSI_SERVICE_ACTION_IN_16:
		produced = cmd_read_capacity_16(cdb, cdb_len, reply);
		ok = produced > 0;
		break;
	case SCSI_READ_FORMAT_CAPACITIES:
		produced = cmd_read_format_capacities(reply);
		break;
	case SCSI_READ_6:
	case SCSI_READ_10:
	case SCSI_READ_12:
	case SCSI_READ_16: {
		uint32_t lba = 0;
		uint32_t count = 0;

		if (!read_range(op, cdb, cdb_len, &lba, &count)) {
			send_csw(tag, host_len, OSK_MSC_CSW_FAILED);
			return;
		}
		/* Der Rechner muss genau so viel erwarten, wie er bestellt
		 * hat. Weniger waere eine halbe Phase, mehr ein Rueckstand,
		 * den kein echter Rechner erzeugt. */
		if (host_len != count * (uint32_t)OSK_MSC_BLOCK_LEN) {
			sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_FIELD, 0x00);
			send_csw(tag, host_len, OSK_MSC_CSW_PHASE);
			return;
		}
		if (count == 0u) {
			send_csw(tag, 0, OSK_MSC_CSW_PASSED);
			return;
		}

		const uint32_t done = send_blocks(lba, count);
		const uint32_t sent = done * (uint32_t)OSK_MSC_BLOCK_LEN;

		send_csw(tag, host_len - sent,
			 (done == count) ? OSK_MSC_CSW_PASSED : OSK_MSC_CSW_FAILED);
		return;
	}
	default:
		sense_set(OSK_MSC_SK_ILLEGAL_REQUEST, OSK_MSC_ASC_INVALID_OPCODE, 0x00);
		ok = false;
		break;
	}

	if (!ok) {
		send_csw(tag, host_len, OSK_MSC_CSW_FAILED);
		return;
	}

	/* Hinaus geht, was der Rechner angefordert hat - nie mehr, als er
	 * erwartet, und nie mehr, als der Befehl erzeugt hat. */
	uint32_t sent = 0;

	if (produced > 0u && host_len > 0u) {
		sent = (host_len < (uint32_t)produced) ? host_len : (uint32_t)produced;
		send(reply, sent);
	}
	memset(reply, 0, sizeof(reply));

	if (op != SCSI_REQUEST_SENSE)
		sense_clear();
	send_csw(tag, host_len - sent, OSK_MSC_CSW_PASSED);
}

void osk_msc_rx(const uint8_t *data, size_t len)
{
	if (!g_on || g_stalled || !data)
		return;

	/* Reste eines abgewiesenen Schreibbefehls. */
	if (g_discard_left > 0u) {
		const uint32_t take =
		    (len < (size_t)g_discard_left) ? (uint32_t)len : g_discard_left;

		g_discard_left -= take;
		if (g_discard_left == 0u)
			send_csw(g_discard_tag, g_discard_total, OSK_MSC_CSW_FAILED);
		return;
	}

	/* Ein CBW ist genau 31 Byte lang und faengt mit seiner Kennung an.
	 * Stimmt eines von beidem nicht, wird nicht geantwortet (BBB 6.6.1). */
	if (len != OSK_MSC_CBW_LEN || get_le32(data) != OSK_MSC_CBW_SIG) {
		g_stalled = true;
		return;
	}

	handle_cbw(data);
}

void osk_msc_reset(void)
{
	g_stalled = false;
	g_discard_left = 0;
	g_discard_total = 0;
	g_discard_tag = 0;
	sense_clear();
}

osk_result_t osk_msc_set_enabled(bool on)
{
	g_on = on;
	if (!on)
		osk_msc_reset();
	return osk_transport_enable(OSK_LINK_USB_MSC, on);
}

bool osk_msc_enabled(void)
{
	return g_on;
}

bool osk_msc_stalled(void)
{
	return g_stalled;
}

uint32_t osk_msc_blocks(void)
{
	return g_blocks;
}

uint32_t osk_msc_write_attempts(void)
{
	return g_write_attempts;
}

void osk_msc_write_attempts_reset(void)
{
	g_write_attempts = 0;
}

/* Der Bearbeiter fuer die Transportliste. Der Rahmen ist derselbe wie bei den
 * anderen Transporten: Bytes herein, Bytes hinaus. Herein kommt ein CBW,
 * hinaus geht das CSW; die Bloecke selbst gehen am Puffer vorbei durch den
 * Sendepfad, weil sie nicht in eine Antwort passen. */
static osk_result_t msc_handle(const osk_request_t *req, osk_response_t *res)
{
	if (!req || !res)
		return OSK_ERR_ARG;
	if (req->link != OSK_LINK_USB_MSC)
		return OSK_ERR_UNSUPPORTED;

	osk_msc_rx(req->data, req->len);
	res->len = 0;
	return OSK_OK;
}

osk_result_t osk_msc_init(const osk_msc_config_t *cfg)
{
	if (!cfg || !cfg->read || !cfg->send || cfg->blocks == 0u)
		return OSK_ERR_ARG;

	g_read = cfg->read;
	g_send = cfg->send;
	g_ctx = cfg->ctx;
	g_blocks = cfg->blocks;
	g_on = true;
	g_stalled = false;
	g_write_attempts = 0;
	g_discard_left = 0;
	g_discard_total = 0;
	g_discard_tag = 0;
	sense_clear();

	return osk_transport_register(OSK_LINK_USB_MSC, msc_handle);
}
