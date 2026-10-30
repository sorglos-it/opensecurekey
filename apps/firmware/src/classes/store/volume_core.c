/* Der Datentraeger: oeffnen, lesen, schreiben, festschreiben - fuer alle drei
 * Betriebsarten in einem Weg. Der Unterschied zwischen ihnen steckt an genau
 * zwei Stellen: wie der Datenschluessel eingewickelt ist (volume_recv.c) und
 * ob ueberhaupt verschluesselt wird.
 *
 * Kein dynamischer Speicher. Die Puffer sind feste Module-Variablen, weil
 * 4-KiB-Seiten auf einem 256-KiB-Rechenkern nicht auf den Stapel gehoeren.
 *
 * AM GERAET UNGEPRUEFT. Es gibt keine Karte, keinen Sicherheitschip und kein
 * Brett. Alles hier laeuft gegen Attrappen. */

#include <string.h>
#include "osk_volume.h"
#include "osk_volume_card.h"
#include "osk_volume_crypto.h"
#include "volume_internal.h"

/* --- Aufbau des Wurzelsatzes ------------------------------------------
 *
 * Feste Stellen, keine Laengenfelder. Gesiegelt wird alles bis zum Siegel. */
#define R_MAGIC 0u	 /* 4 */
#define R_FORMAT 4u	 /* 2 */
#define R_MODE 6u	 /* 2 */
#define R_VERSION 8u	 /* 4  Fassungsnummer, nur aufwaerts */
#define R_TIME 12u	 /* 8  Zeitstempel des Absenders */
#define R_BLOCKS 20u	 /* 4 */
#define R_ID 24u	 /* 16 Datentraegerkennung */
#define R_ROOTHASH 40u	 /* 32 */
#define R_SENDER 72u	 /* 32 oeffentlicher Absenderschluessel */
#define R_COUNTER 104u	 /* 8  Zielzaehlerstand, nur Fall A */
#define R_SEALED 112u	 /* 1  versiegelt (Modus 3) */
#define R_RECVCOUNT 113u /* 1 */
#define R_RESERVE 114u	 /* 2 */
#define R_RECV 116u	 /* OSK_VOL_RECV_MAX Zeilen */
#define R_SEAL (R_RECV + OSK_VOL_RECV_MAX * OSK_VOL_RECV_LINE_LEN)
#define R_END (R_SEAL + OSK_VOL_SIG_LEN)

/* --- Zustand ---------------------------------------------------------- */

static struct {
	bool inited;
	bool open;
	osk_volume_config_t cfg;

	osk_vol_mode_t mode;
	osk_vol_layout_t lay;
	uint8_t id[OSK_VOL_ID_LEN];
	uint32_t version;
	uint64_t stamped_at;
	uint64_t counter_target;
	uint8_t root_hash[OSK_HASH_LEN];
	uint8_t sender_pub[OSK_VOL_PUB_LEN];
	bool sealed;
	uint8_t recv_count;
	uint8_t recv[OSK_VOL_RECV_MAX][OSK_VOL_RECV_LINE_LEN];

	/* Datenschluessel. Liegt nur waehrend der Sitzung im RAM, und nur in
	 * Modus 1 und 2 - in Modus 3 gibt es keinen. */
	osk_key_t k_blocks;
	bool have_key;

	bool write_enabled;
	uint32_t dirty;
	uint8_t active; /* welcher Wurzelsatz gilt: 0 oder 1 */

	/* Spitze des Baums im RAM. Siehe cached_levels. */
	uint8_t cache[OSK_VOL_LEVELS_MAX][OSK_VOL_BLOCK_LEN];
	uint32_t cache_pages;
} v;

/* Feste Arbeitspuffer. Kein dynamischer Speicher in Krypto-Pfaden. */
static uint8_t page_a[OSK_VOL_BLOCK_LEN];
static uint8_t page_b[OSK_VOL_BLOCK_LEN];
static uint8_t root_buf[OSK_VOL_BLOCK_LEN];
/* Zusaetzliche Daten je Block: Blocknummer, Datentraegerkennung, Modus,
 * Formatfassung. Damit laesst sich ein Block nicht an eine andere Stelle und
 * nicht auf eine andere Karte verschieben. */
#define VOL_AAD_LEN (4u + OSK_VOL_ID_LEN + 1u + 2u)

/* Platz fuer einen Block und dahinter entweder den Stempel (Modus 1 und 2)
 * oder die zusaetzlichen Daten, die in Modus 3 mitgehasht werden. Der
 * groessere von beiden bestimmt die Groesse - einmal hingeschrieben. */
#define VOL_CT_TAIL ((VOL_AAD_LEN > OSK_VOL_AEAD_TAG_LEN) ? VOL_AAD_LEN : OSK_VOL_AEAD_TAG_LEN)
static uint8_t ct_buf[OSK_VOL_BLOCK_LEN + VOL_CT_TAIL];
/* Eigener Puffer fuer die vollstaendige Pruefung. page_a und page_b gehoeren
 * der Pfadpruefung; wer sie als Ziel eines Lesevorgangs nimmt, zieht ihr den
 * Boden weg. */
static uint8_t scratch[OSK_VOL_BLOCK_LEN];

/* --- kleine Helfer ---------------------------------------------------- */

/* Weiter unten, aber vom Formatieren gebraucht. */
static osk_result_t block_put(uint32_t block, const uint8_t *in);

static void put32(uint8_t *p, uint32_t x)
{
	p[0] = (uint8_t)(x & 0xffu);
	p[1] = (uint8_t)((x >> 8) & 0xffu);
	p[2] = (uint8_t)((x >> 16) & 0xffu);
	p[3] = (uint8_t)((x >> 24) & 0xffu);
}

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static void put64(uint8_t *p, uint64_t x)
{
	put32(p, (uint32_t)(x & 0xffffffffu));
	put32(p + 4, (uint32_t)(x >> 32));
}

static uint64_t get64(const uint8_t *p)
{
	return (uint64_t)get32(p) | ((uint64_t)get32(p + 4) << 32);
}

/* Zusaetzliche Daten je Block: Blocknummer, Datentraegerkennung, Modus,
 * Formatfassung. Damit laesst sich ein Block nicht an eine andere Stelle und
 * nicht auf eine andere Karte verschieben. */

static void build_aad(uint32_t block, uint8_t out[VOL_AAD_LEN])
{
	put32(out, block);
	memcpy(out + 4, v.id, OSK_VOL_ID_LEN);
	out[4 + OSK_VOL_ID_LEN] = (uint8_t)v.mode;
	out[5 + OSK_VOL_ID_LEN] = (uint8_t)(OSK_VOL_FORMAT_VERSION & 0xffu);
	out[6 + OSK_VOL_ID_LEN] = (uint8_t)(OSK_VOL_FORMAT_VERSION >> 8);
}

static void build_nonce(uint32_t write_count, uint32_t block, uint32_t purpose,
			uint8_t out[OSK_VOL_AEAD_NONCE_LEN])
{
	put32(out, write_count);
	put32(out + 4, block);
	put32(out + 8, purpose);
}

/* --- Baum ------------------------------------------------------------- */

/* Ist diese Ebene im RAM gemerkt? Die obersten cached_levels sind es. */
static bool level_cached(uint32_t lvl)
{
	return lvl >= (v.lay.levels - v.lay.cached_levels);
}

static uint32_t cache_slot(uint32_t lvl, uint32_t idx)
{
	uint32_t slot = 0u;
	uint32_t i;

	for (i = v.lay.levels - v.lay.cached_levels; i < lvl; i++) {
		slot += v.lay.level_pages[i];
	}
	return slot + idx;
}

static osk_result_t level_page_get(uint32_t lvl, uint32_t idx, uint8_t out[OSK_VOL_BLOCK_LEN])
{
	if (level_cached(lvl)) {
		uint32_t slot = cache_slot(lvl, idx);

		if (slot >= v.cache_pages) {
			return OSK_ERR_INTERNAL;
		}
		memcpy(out, v.cache[slot], OSK_VOL_BLOCK_LEN);
		return OSK_OK;
	}
	return osk_card_read(v.lay.first_level_page[lvl] + idx, out, OSK_VOL_BLOCK_LEN);
}

static osk_result_t level_page_put(uint32_t lvl, uint32_t idx, const uint8_t in[OSK_VOL_BLOCK_LEN])
{
	osk_result_t r = osk_card_write(v.lay.first_level_page[lvl] + idx, in, OSK_VOL_BLOCK_LEN);

	if (r != OSK_OK) {
		return r;
	}
	if (level_cached(lvl)) {
		uint32_t slot = cache_slot(lvl, idx);

		if (slot >= v.cache_pages) {
			return OSK_ERR_INTERNAL;
		}
		memcpy(v.cache[slot], in, OSK_VOL_BLOCK_LEN);
	}
	return OSK_OK;
}

/* Den Baum von unten nach oben neu rechnen und den Wurzelhash setzen.
 *
 * EHRLICH GESAGT: das rechnet den GANZEN Baum neu, nicht nur die Pfade der
 * geaenderten Bloecke. Fuer eine 16-GiB-Karte sind das 32 768 Stempelseiten,
 * also 129 MiB Hashen je Festschreibung - bei 25,8 MB/s rund 5 Sekunden, und
 * das ist zu viel fuer die Obergrenze von 1024 Bloecken. Das Nachziehen nur
 * der betroffenen Seiten ist dieselbe Rechnung von unten nach oben, nur auf
 * den beruehrten Seiten, und ist NICHT gebaut. Offener Punkt, dem Master
 * gemeldet. */
static osk_result_t tree_rebuild(void)
{
	uint32_t lvl;
	uint32_t below;
	uint32_t idx;
	osk_result_t r;

	below = v.lay.leaf_pages;
	for (lvl = 0u; lvl < v.lay.levels; lvl++) {
		for (idx = 0u; idx < v.lay.level_pages[lvl]; idx++) {
			uint32_t child;

			memset(page_a, 0, sizeof(page_a));
			for (child = 0u; child < OSK_VOL_FANOUT; child++) {
				uint32_t ci = idx * OSK_VOL_FANOUT + child;

				if (ci >= below) {
					break;
				}
				if (lvl == 0u) {
					r = osk_card_read(v.lay.first_leaf_page + ci, page_b,
							  OSK_VOL_BLOCK_LEN);
				} else {
					r = level_page_get(lvl - 1u, ci, page_b);
				}
				if (r != OSK_OK) {
					return r;
				}
				r = osk_crypto_sha256(page_b, OSK_VOL_BLOCK_LEN,
						      page_a + child * OSK_HASH_LEN);
				if (r != OSK_OK) {
					return r;
				}
			}
			r = level_page_put(lvl, idx, page_a);
			if (r != OSK_OK) {
				return r;
			}
		}
		below = v.lay.level_pages[lvl];
	}
	/* Der Wurzelhash ist der Hash der obersten Seite. Sie liegt im RAM. */
	r = level_page_get(v.lay.levels - 1u, 0u, page_a);
	if (r != OSK_OK) {
		return r;
	}
	return osk_crypto_sha256(page_a, OSK_VOL_BLOCK_LEN, v.root_hash);
}

/* Die oberen Ebenen beim Oeffnen in den RAM holen. */
static osk_result_t cache_load(void)
{
	uint32_t lvl;
	uint32_t idx;
	osk_result_t r;

	v.cache_pages = 0u;
	for (lvl = v.lay.levels - v.lay.cached_levels; lvl < v.lay.levels; lvl++) {
		for (idx = 0u; idx < v.lay.level_pages[lvl]; idx++) {
			if (v.cache_pages >= OSK_VOL_LEVELS_MAX) {
				return OSK_ERR_INTERNAL;
			}
			r = osk_card_read(v.lay.first_level_page[lvl] + idx, v.cache[v.cache_pages],
					  OSK_VOL_BLOCK_LEN);
			if (r != OSK_OK) {
				return r;
			}
			v.cache_pages++;
		}
	}
	return OSK_OK;
}

/* Den Pfad EINES Blockes pruefen: Stempelseite gegen Ebene 1, Ebene 1 gegen
 * Ebene 2, und die Spitze gegen den Wurzelhash aus dem gesiegelten Satz.
 * Liefert das Blatt des Blockes heraus - Stempel und Schreibzaehler. */
static osk_result_t path_check(uint32_t block, uint8_t leaf_out[OSK_VOL_LEAF_ENTRY_LEN])
{
	uint32_t child_idx = block / OSK_VOL_LEAVES_PER_PAGE;
	uint32_t lvl;
	uint8_t hash[OSK_HASH_LEN];
	osk_result_t r;

	r = osk_card_read(vol_leaf_page_of(&v.lay, block), page_a, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	memcpy(leaf_out, page_a + vol_leaf_offset_of(block), OSK_VOL_LEAF_ENTRY_LEN);

	for (lvl = 0u; lvl < v.lay.levels; lvl++) {
		uint32_t parent_idx = child_idx / OSK_VOL_FANOUT;
		uint32_t slot = child_idx % OSK_VOL_FANOUT;

		r = osk_crypto_sha256(page_a, OSK_VOL_BLOCK_LEN, hash);
		if (r != OSK_OK) {
			return r;
		}
		r = level_page_get(lvl, parent_idx, page_b);
		if (r != OSK_OK) {
			return r;
		}
		if (!osk_crypto_equal(hash, page_b + slot * OSK_HASH_LEN, OSK_HASH_LEN)) {
			return OSK_ERR_FORMAT;
		}
		memcpy(page_a, page_b, OSK_VOL_BLOCK_LEN);
		child_idx = parent_idx;
	}
	r = osk_crypto_sha256(page_a, OSK_VOL_BLOCK_LEN, hash);
	if (r != OSK_OK) {
		return r;
	}
	if (!osk_crypto_equal(hash, v.root_hash, OSK_HASH_LEN)) {
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}

/* --- Wurzelsatz ------------------------------------------------------- */

static void root_serialize(uint8_t *buf)
{
	uint32_t i;

	memset(buf, 0, OSK_VOL_BLOCK_LEN);
	put32(buf + R_MAGIC, OSK_VOL_MAGIC);
	buf[R_FORMAT] = (uint8_t)OSK_VOL_FORMAT_VERSION;
	buf[R_MODE] = (uint8_t)v.mode;
	put32(buf + R_VERSION, v.version);
	put64(buf + R_TIME, v.stamped_at);
	put32(buf + R_BLOCKS, v.lay.data_blocks);
	memcpy(buf + R_ID, v.id, OSK_VOL_ID_LEN);
	memcpy(buf + R_ROOTHASH, v.root_hash, OSK_HASH_LEN);
	memcpy(buf + R_SENDER, v.sender_pub, OSK_VOL_PUB_LEN);
	put64(buf + R_COUNTER, v.counter_target);
	buf[R_SEALED] = v.sealed ? 1u : 0u;
	buf[R_RECVCOUNT] = v.recv_count;
	for (i = 0u; i < v.recv_count; i++) {
		memcpy(buf + R_RECV + i * OSK_VOL_RECV_LINE_LEN, v.recv[i], OSK_VOL_RECV_LINE_LEN);
	}
}

/* Das Siegel. Fall A (Modus 1): HMAC im Chip, weil Pruefer und Stempler
 * dasselbe Geraet sind. Fall B (Modus 2 und 3): Ed25519, weil ein ANDERER
 * Stick prueft und ein geteilter Schluessel dort kein Geheimnis waere. */
static osk_result_t root_seal(uint8_t *buf)
{
	size_t len = 0u;

	if (v.mode == OSK_VOL_MODE_DEVICE) {
		return osk_se050_hmac(v.cfg.root_key, buf, R_SEAL, buf + R_SEAL, OSK_VOL_SIG_LEN,
				      &len);
	}
	uint8_t hash[OSK_HASH_LEN];
	osk_result_t r = osk_crypto_sha256(buf, R_SEAL, hash);

	if (r != OSK_OK) {
		return r;
	}
	return osk_se050_sign(v.cfg.sign_key, hash, OSK_HASH_LEN, buf + R_SEAL, OSK_VOL_SIG_LEN,
			      &len);
}

static osk_result_t root_seal_check(const uint8_t *buf, osk_vol_mode_t mode)
{
	if (mode == OSK_VOL_MODE_DEVICE) {
		uint8_t mac[OSK_VOL_SIG_LEN];
		size_t len = 0u;
		osk_result_t r =
		    osk_se050_hmac(v.cfg.root_key, buf, R_SEAL, mac, sizeof(mac), &len);

		if (r != OSK_OK) {
			return r;
		}
		/* Ein fremder Stick hat ein anderes Grundgeheimnis und kommt
		 * hier auf einen anderen MAC. Das ist kein Formatfehler,
		 * sondern der falsche Stick: OSK_ERR_DENIED. */
		if (!osk_crypto_equal(mac, buf + R_SEAL, len)) {
			return OSK_ERR_DENIED;
		}
		return OSK_OK;
	}
	uint8_t hash[OSK_HASH_LEN];
	osk_result_t r = osk_crypto_sha256(buf, R_SEAL, hash);

	if (r != OSK_OK) {
		return r;
	}
	return osk_vol_sig_verify(buf + R_SENDER, hash, OSK_HASH_LEN, buf + R_SEAL);
}

/* Beide Saetze lesen und den gueltigen mit der HOEHEREN Fassungsnummer
 * waehlen. In Fall B ist der signierte Satz selbst der Festschreibepunkt. */
static osk_result_t root_load(void)
{
	uint8_t which;
	int best = -1;
	uint32_t best_ver = 0u;
	osk_result_t r;

	for (which = 0u; which < 2u; which++) {
		osk_vol_mode_t mode;
		uint32_t ver;

		r = osk_card_read(v.lay.root_page[which], root_buf, OSK_VOL_BLOCK_LEN);
		if (r != OSK_OK) {
			continue;
		}
		if (get32(root_buf + R_MAGIC) != OSK_VOL_MAGIC) {
			continue;
		}
		if (root_buf[R_FORMAT] != (uint8_t)OSK_VOL_FORMAT_VERSION) {
			continue;
		}
		mode = (osk_vol_mode_t)root_buf[R_MODE];
		if (mode != OSK_VOL_MODE_DEVICE && mode != OSK_VOL_MODE_WORD &&
		    mode != OSK_VOL_MODE_SEALED) {
			continue;
		}
		if (root_buf[R_RECVCOUNT] > OSK_VOL_RECV_MAX) {
			continue;
		}
		ver = get32(root_buf + R_VERSION);
		if (best < 0 || ver > best_ver) {
			best = (int)which;
			best_ver = ver;
		}
	}
	if (best < 0) {
		return OSK_ERR_FORMAT;
	}
	r = osk_card_read(v.lay.root_page[best], root_buf, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	v.active = (uint8_t)best;
	return OSK_OK;
}

static void root_take(const uint8_t *buf)
{
	uint32_t i;

	v.mode = (osk_vol_mode_t)buf[R_MODE];
	v.version = get32(buf + R_VERSION);
	v.stamped_at = get64(buf + R_TIME);
	memcpy(v.id, buf + R_ID, OSK_VOL_ID_LEN);
	memcpy(v.root_hash, buf + R_ROOTHASH, OSK_HASH_LEN);
	memcpy(v.sender_pub, buf + R_SENDER, OSK_VOL_PUB_LEN);
	v.counter_target = get64(buf + R_COUNTER);
	v.sealed = buf[R_SEALED] != 0u;
	v.recv_count = buf[R_RECVCOUNT];
	for (i = 0u; i < v.recv_count; i++) {
		memcpy(v.recv[i], buf + R_RECV + i * OSK_VOL_RECV_LINE_LEN, OSK_VOL_RECV_LINE_LEN);
	}
}

/* --- Schnittstelle ---------------------------------------------------- */

osk_result_t osk_volume_init(const osk_volume_config_t *cfg)
{
	if (cfg == NULL || cfg->ui_confirm == NULL) {
		/* Ohne Anzeige keine Schreibfreigabe - und damit kein
		 * Datentraeger. Das ist die Auflage aus dem Entwurf, und sie
		 * gilt schon beim Einrichten, nicht erst beim Schreiben. */
		return OSK_ERR_ARG;
	}
	memset(&v, 0, sizeof(v));
	v.cfg = *cfg;
	v.inited = true;
	return OSK_OK;
}

void osk_volume_close(void)
{
	/* Schluessel im RAM ueberschreiben. Danach gibt es ohne neues Oeffnen
	 * keinen Weg mehr an die Daten. */
	osk_crypto_wipe(&v.k_blocks, sizeof(v.k_blocks));
	v.have_key = false;
	v.open = false;
	v.write_enabled = false;
	v.dirty = 0u;
}

bool osk_volume_is_open(void)
{
	return v.open;
}

osk_vol_mode_t osk_volume_mode(void)
{
	return v.mode;
}

bool osk_volume_write_enabled(void)
{
	return v.write_enabled;
}

uint32_t osk_volume_dirty_blocks(void)
{
	return v.dirty;
}

bool osk_volume_sealed(void)
{
	return v.sealed;
}

/* Die Auflage: der Modus wird genannt, BEVOR geschrieben wird. Eine
 * Rueckmeldung, die nicht OSK_OK liefert, heiszt NEIN - nicht "dann eben
 * ohne". */
static osk_result_t ask_mode(osk_vol_mode_t mode, uint32_t recipients)
{
	if (v.cfg.ui_confirm == NULL) {
		return OSK_ERR_DENIED;
	}
	return v.cfg.ui_confirm(mode, recipients, osk_vol_mode_secret(mode), v.cfg.ui_ctx);
}

osk_result_t osk_volume_format(osk_vol_mode_t mode, const osk_vol_recipient_t *recv,
			       uint32_t recv_count, uint32_t data_blocks)
{
	osk_card_info_t info;
	osk_result_t r;
	uint32_t i;
	osk_key_t k_data;

	if (!v.inited) {
		return OSK_ERR_INTERNAL;
	}
	if (mode != OSK_VOL_MODE_DEVICE && mode != OSK_VOL_MODE_WORD &&
	    mode != OSK_VOL_MODE_SEALED) {
		return OSK_ERR_ARG;
	}
	if (recv_count > OSK_VOL_RECV_MAX) {
		return OSK_ERR_ARG;
	}
	/* Modus 3 hat keine Empfaengerzeile und keinen Datenschluessel. Wer
	 * eine mitgibt, meint etwas anderes als Modus 3 - das ist genau die
	 * Verwechslung, die hier nicht durchgehen darf. */
	if (mode == OSK_VOL_MODE_SEALED && recv_count != 0u) {
		return OSK_ERR_ARG;
	}
	if (mode != OSK_VOL_MODE_SEALED && recv_count == 0u) {
		return OSK_ERR_ARG;
	}
	r = osk_card_info(&info);
	if (r != OSK_OK) {
		return r;
	}
	if (!info.present) {
		return OSK_ERR_NOT_FOUND;
	}
	if (info.write_protected) {
		return OSK_ERR_DENIED;
	}
	r = osk_vol_layout(data_blocks, &v.lay);
	if (r != OSK_OK) {
		return r;
	}
	if (v.lay.total_pages > info.blocks) {
		return OSK_ERR_FULL;
	}
	r = ask_mode(mode, recv_count);
	if (r != OSK_OK) {
		return r;
	}

	osk_volume_close();
	v.mode = mode;
	v.version = 1u;
	v.stamped_at = 0u;
	v.sealed = false;
	v.recv_count = 0u;
	memset(v.recv, 0, sizeof(v.recv));
	memset(v.sender_pub, 0, sizeof(v.sender_pub));

	r = osk_crypto_random(v.id, OSK_VOL_ID_LEN);
	if (r != OSK_OK) {
		return r;
	}

	if (mode == OSK_VOL_MODE_SEALED) {
		/* Kein Datenschluessel. Die Bloecke liegen im Klartext, das
		 * Blatt ist der Anfang ihres SHA-256. */
		v.have_key = false;
	} else {
		/* K_daten ist GEWUERFELT, nicht abgeleitet - genau deshalb
		 * laesst er sich mehrfach einwickeln. */
		r = osk_crypto_random(k_data.b, OSK_KEY_LEN);
		if (r != OSK_OK) {
			return r;
		}
		for (i = 0u; i < recv_count; i++) {
			r = vol_recv_wrap(&v.cfg, &recv[i], v.id, &k_data, v.recv[v.recv_count]);
			if (r != OSK_OK) {
				osk_crypto_wipe(&k_data, sizeof(k_data));
				return r;
			}
			v.recv_count++;
		}
		r = osk_crypto_hkdf(k_data.b, OSK_KEY_LEN, v.id, OSK_VOL_ID_LEN,
				    (const uint8_t *)"osk/volume/data", 15u, v.k_blocks.b,
				    OSK_KEY_LEN);
		osk_crypto_wipe(&k_data, sizeof(k_data));
		if (r != OSK_OK) {
			return r;
		}
		v.have_key = true;
	}

	/* Absenderschluessel: in Fall B steht er in der Wurzel, weil der
	 * Empfaenger ihn braucht. In Modus 1 bleibt er leer - dort prueft
	 * derselbe Stick mit einem MAC. */
	if (mode != OSK_VOL_MODE_DEVICE) {
		size_t len = 0u;

		r = osk_se050_key_public(v.cfg.sign_key, v.sender_pub, OSK_VOL_PUB_LEN, &len);
		if (r != OSK_OK) {
			return r;
		}
	}

	/* Leere Karte: Stempelfeld und Bloecke auf null, dann Baum und
	 * Wurzel. Ein frisch formatierter Datentraeger ist in sich stimmig. */
	memset(page_a, 0, sizeof(page_a));
	for (i = 0u; i < v.lay.leaf_pages; i++) {
		r = osk_card_write(v.lay.first_leaf_page + i, page_a, OSK_VOL_BLOCK_LEN);
		if (r != OSK_OK) {
			return r;
		}
	}
	v.cache_pages = 0u;
	for (i = v.lay.levels - v.lay.cached_levels; i < v.lay.levels; i++) {
		v.cache_pages += v.lay.level_pages[i];
	}
	v.open = true;

	/* JEDEN Block beschreiben, auch die leeren. Ein Block ohne Stempel
	 * laesst sich nicht lesen, und das Betriebssystem liest Sektoren, in
	 * die es nie geschrieben hat - eine frisch formatierte Karte waere
	 * sonst sofort kaputt.
	 *
	 * DER PREIS, und er ist unangenehm: Formatieren geht einmal ueber die
	 * ganze Karte. Bei 16 GiB und rund 3 MB/s von der Karte sind das etwa
	 * 1,5 Stunden, plus das Verschluesseln. Die Anzeige muss diese Dauer
	 * VORHER nennen, so wie die vollstaendige Pruefung ihre nennt.
	 * Gerechnet, nicht gemessen - es gibt keine Karte. */
	memset(page_a, 0, sizeof(page_a));
	for (i = 0u; i < data_blocks; i++) {
		r = block_put(i, page_a);
		if (r != OSK_OK) {
			v.open = false;
			return r;
		}
	}
	r = tree_rebuild();
	if (r != OSK_OK) {
		v.open = false;
		return r;
	}
	v.active = 1u; /* commit schreibt in den inaktiven Satz, also 0 */
	v.dirty = 0u;
	r = osk_volume_commit();
	if (r != OSK_OK) {
		v.open = false;
		return r;
	}
	return OSK_OK;
}

osk_result_t osk_volume_open(const osk_vol_recipient_t *unlock)
{
	osk_card_info_t info;
	osk_result_t r;
	osk_key_t k_data;
	uint32_t i;
	bool opened = false;

	if (!v.inited) {
		return OSK_ERR_INTERNAL;
	}
	r = osk_card_info(&info);
	if (r != OSK_OK) {
		return r;
	}
	if (!info.present) {
		return OSK_ERR_NOT_FOUND;
	}
	osk_volume_close();

	/* Der Aufbau folgt aus der Blockzahl in der Wurzel. Die steht im
	 * gesiegelten Teil - ein veraenderter Wert faellt am Siegel auf. */
	r = osk_vol_layout(1u, &v.lay);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_card_read(0u, root_buf, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	if (get32(root_buf + R_MAGIC) != OSK_VOL_MAGIC) {
		return OSK_ERR_FORMAT;
	}
	r = osk_vol_layout(get32(root_buf + R_BLOCKS), &v.lay);
	if (r != OSK_OK) {
		return r;
	}
	r = root_load();
	if (r != OSK_OK) {
		return r;
	}
	r = root_seal_check(root_buf, (osk_vol_mode_t)root_buf[R_MODE]);
	if (r != OSK_OK) {
		return r;
	}
	root_take(root_buf);

	v.cache_pages = 0u;
	for (i = v.lay.levels - v.lay.cached_levels; i < v.lay.levels; i++) {
		v.cache_pages += v.lay.level_pages[i];
	}
	r = cache_load();
	if (r != OSK_OK) {
		return r;
	}

	if (v.mode == OSK_VOL_MODE_SEALED) {
		/* Nichts auszuwickeln. Modus 3 ist nicht geheim. */
		v.have_key = false;
		v.open = true;
		return OSK_OK;
	}

	/* Eine Zeile muss passen. Der Chipschluessel wird immer versucht,
	 * das Kartenwort nur, wenn es mitgegeben wurde. */
	for (i = 0u; i < v.recv_count && !opened; i++) {
		r = vol_recv_unwrap(&v.cfg, v.recv[i], v.id, unlock, &k_data);
		if (r == OSK_OK) {
			opened = true;
		}
	}
	if (!opened) {
		/* Falscher Stick oder falsches Wort. KEIN Formatfehler. */
		return OSK_ERR_DENIED;
	}
	r = osk_crypto_hkdf(k_data.b, OSK_KEY_LEN, v.id, OSK_VOL_ID_LEN,
			    (const uint8_t *)"osk/volume/data", 15u, v.k_blocks.b, OSK_KEY_LEN);
	osk_crypto_wipe(&k_data, sizeof(k_data));
	if (r != OSK_OK) {
		return r;
	}
	v.have_key = true;
	v.open = true;
	return OSK_OK;
}

osk_result_t osk_volume_root_unchanged(void)
{
	osk_result_t r;
	uint8_t hash[OSK_HASH_LEN];

	if (!v.open) {
		return OSK_ERR_DENIED;
	}
	r = osk_card_read(v.lay.root_page[v.active], root_buf, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	r = root_seal_check(root_buf, v.mode);
	if (r != OSK_OK) {
		return r;
	}
	/* Die Bindung. Fall A: der Zaehler im Chip. Ein Satz mit Zielstand
	 * C+1 bei Stand C gilt als vorbereitet und wird angenommen - der
	 * Sprung ist auf genau eins begrenzt. */
	if (v.mode == OSK_VOL_MODE_DEVICE) {
		uint64_t c = 0u;

		r = osk_se050_counter_read(v.cfg.counter_id, &c);
		if (r != OSK_OK) {
			return r;
		}
		if (v.counter_target < c) {
			/* Der Datentraeger ist in sich stimmig und ALT. Das ist
			 * ein Rueckdreh und kein Formatfehler - wer die beiden
			 * zusammenwirft, meldet einen kaputten Datentraeger, wo
			 * ein Angriff vorliegt. */
			return OSK_ERR_ROLLBACK;
		}
		if (v.counter_target == c + 1u) {
			/* Vorbereitet: der Satz steht, der Zaehlerschritt
			 * fehlte. Wird angenommen und nachgezogen. Ein
			 * Angreifer kann so einen Satz nicht bauen - dazu
			 * braeuchte er den Schluessel aus dem Chip. */
			r = osk_se050_counter_raise(v.cfg.counter_id, v.counter_target);
			if (r != OSK_OK) {
				return r;
			}
		} else if (v.counter_target > c + 1u) {
			/* Mehr als ein Schritt wird NIE angenommen. */
			return OSK_ERR_FORMAT;
		}
	}
	/* Und die oberste Baumseite gegen den Wurzelhash. Das ist alles, was
	 * beim Einstecken geprueft wird - NICHT die 4 Millionen Bloecke. */
	r = level_page_get(v.lay.levels - 1u, 0u, page_a);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_crypto_sha256(page_a, OSK_VOL_BLOCK_LEN, hash);
	if (r != OSK_OK) {
		return r;
	}
	if (!osk_crypto_equal(hash, v.root_hash, OSK_HASH_LEN)) {
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}

osk_result_t osk_volume_sender(osk_vol_sender_t *out)
{
	osk_result_t r;
	uint8_t own[OSK_VOL_PUB_LEN];
	size_t len = 0u;

	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!v.open) {
		return OSK_ERR_DENIED;
	}
	memset(out, 0, sizeof(*out));
	out->version = v.version;
	out->stamped_at = v.stamped_at;

	if (v.mode == OSK_VOL_MODE_DEVICE) {
		/* Fall A: es gibt keinen Absenderschluessel, weil derselbe
		 * Stick prueft. Trotzdem SELF und nicht "bekannt" - der
		 * Unterschied ist, dass hier gar niemand weitergegeben hat. */
		out->state = OSK_VOL_SENDER_SELF;
		return OSK_OK;
	}
	r = osk_crypto_sha256(v.sender_pub, OSK_VOL_PUB_LEN, out->fingerprint);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_se050_key_public(v.cfg.sign_key, own, sizeof(own), &len);
	if (r == OSK_OK && len == OSK_VOL_PUB_LEN &&
	    osk_crypto_equal(own, v.sender_pub, OSK_VOL_PUB_LEN)) {
		out->state = OSK_VOL_SENDER_SELF;
		return OSK_OK;
	}
	/* HIER ENDET, WAS DIE SIGNATUR BEWEIST. Dass dieser Schluessel zu
	 * einem Menschen gehoert, weiss nur, wer ihn vorher kennt. Die Tabelle
	 * der bekannten Absender liegt im Tresor und gehoert dem store-Strang;
	 * solange sie nicht da ist, ist die Antwort UNKNOWN - und das ist die
	 * Wahrheit, nicht eine Luecke. */
	out->state = OSK_VOL_SENDER_UNKNOWN;
	return OSK_OK;
}

osk_result_t osk_volume_write_enable(void)
{
	osk_result_t r;

	if (!v.open) {
		return OSK_ERR_DENIED;
	}
	if (v.sealed) {
		/* Versiegelt heiszt schreibgeschuetzt. Entsiegeln ist ein
		 * eigener Vorgang und macht die alte Signatur ungueltig. */
		return OSK_ERR_DENIED;
	}
	r = ask_mode(v.mode, v.recv_count);
	if (r != OSK_OK) {
		v.write_enabled = false;
		return r;
	}
	v.write_enabled = true;
	return OSK_OK;
}

/* Ein Block schreiben, OHNE die Freigabe zu pruefen. Nur fuer den Aufrufer
 * innerhalb dieser Datei: osk_volume_block_write() prueft vorher, und
 * osk_volume_format() hat die Betriebsart gerade bestaetigen lassen. */
static osk_result_t block_put(uint32_t block, const uint8_t *in)
{
	uint8_t aad[VOL_AAD_LEN];
	uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN];
	uint8_t *leaf;
	uint32_t count;
	size_t ct_len = 0u;
	osk_result_t r;

	if (!v.open || in == NULL) {
		return OSK_ERR_ARG;
	}
	if (block >= v.lay.data_blocks) {
		return OSK_ERR_ARG;
	}

	r = osk_card_read(vol_leaf_page_of(&v.lay, block), page_a, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	leaf = page_a + vol_leaf_offset_of(block);
	count = get32(leaf + OSK_VOL_LEAF_LEN);
	if (count == 0xffffffffu) {
		/* Der Schreibzaehler je Block laeuft ueber. Weiterschreiben
		 * hiesse, eine Nonce zu wiederholen - der eine Fehler, der
		 * Poly1305 vollstaendig bricht. Lieber abweisen. Was dann zu
		 * tun ist (neu verschluesseln), ist NICHT entworfen; offener
		 * Punkt 3 des Konzepts. */
		return OSK_ERR_FULL;
	}
	count++;
	build_aad(block, aad);

	if (v.have_key) {
		build_nonce(count, block, OSK_VOL_PURPOSE_DATA, nonce);
		r = osk_vol_aead_seal(&v.k_blocks, nonce, aad, sizeof(aad), in, OSK_VOL_BLOCK_LEN,
				      ct_buf, sizeof(ct_buf), &ct_len);
		if (r != OSK_OK) {
			return r;
		}
		if (ct_len != OSK_VOL_BLOCK_LEN + OSK_VOL_AEAD_TAG_LEN) {
			return OSK_ERR_INTERNAL;
		}
		/* Der Stempel steht NICHT im Datenblock, sondern im
		 * Stempelfeld - dort ist er das Blatt des Baums. */
		memcpy(leaf, ct_buf + OSK_VOL_BLOCK_LEN, OSK_VOL_LEAF_LEN);
	} else {
		uint8_t hash[OSK_HASH_LEN];

		/* Modus 3: Klartext. An die Stelle des Stempels tritt der
		 * Anfang des SHA-256 ueber Block und zusaetzliche Daten. */
		memcpy(ct_buf, in, OSK_VOL_BLOCK_LEN);
		memcpy(ct_buf + OSK_VOL_BLOCK_LEN, aad, VOL_AAD_LEN);
		r = osk_crypto_sha256(ct_buf, OSK_VOL_BLOCK_LEN + VOL_AAD_LEN, hash);
		if (r != OSK_OK) {
			return r;
		}
		memcpy(leaf, hash, OSK_VOL_LEAF_LEN);
	}
	put32(leaf + OSK_VOL_LEAF_LEN, count);

	r = osk_card_write(v.lay.first_data_page + block, ct_buf, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_card_write(vol_leaf_page_of(&v.lay, block), page_a, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	v.dirty++;
	return OSK_OK;
}

osk_result_t osk_volume_block_write(uint32_t block, const uint8_t in[OSK_VOL_BLOCK_LEN])
{
	if (!v.open || in == NULL) {
		return OSK_ERR_ARG;
	}
	if (!v.write_enabled) {
		/* Kein Modus bestaetigt, keine Schreibfreigabe, kein Byte. */
		return OSK_ERR_DENIED;
	}
	if (v.sealed) {
		return OSK_ERR_DENIED;
	}
	return block_put(block, in);
}

osk_result_t osk_volume_block_read(uint32_t block, uint8_t out[OSK_VOL_BLOCK_LEN])
{
	uint8_t leaf[OSK_VOL_LEAF_ENTRY_LEN];
	uint8_t aad[VOL_AAD_LEN];
	uint8_t nonce[OSK_VOL_AEAD_NONCE_LEN];
	size_t pt_len = 0u;
	osk_result_t r;

	if (!v.open || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (block >= v.lay.data_blocks) {
		return OSK_ERR_ARG;
	}
	/* Erst der Pfad, dann die Daten. Nicht abwaehlbar. */
	r = path_check(block, leaf);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_card_read(v.lay.first_data_page + block, ct_buf, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	build_aad(block, aad);

	if (v.have_key) {
		memcpy(ct_buf + OSK_VOL_BLOCK_LEN, leaf, OSK_VOL_LEAF_LEN);
		build_nonce(get32(leaf + OSK_VOL_LEAF_LEN), block, OSK_VOL_PURPOSE_DATA, nonce);
		r = osk_vol_aead_open(&v.k_blocks, nonce, aad, sizeof(aad), ct_buf,
				      OSK_VOL_BLOCK_LEN + OSK_VOL_AEAD_TAG_LEN, out,
				      OSK_VOL_BLOCK_LEN, &pt_len);
		if (r != OSK_OK) {
			return r;
		}
		if (pt_len != OSK_VOL_BLOCK_LEN) {
			return OSK_ERR_INTERNAL;
		}
		return OSK_OK;
	}
	/* Modus 3: der Hash im Blatt gegen den Block. */
	uint8_t hash[OSK_HASH_LEN];

	memcpy(ct_buf + OSK_VOL_BLOCK_LEN, aad, VOL_AAD_LEN);
	r = osk_crypto_sha256(ct_buf, OSK_VOL_BLOCK_LEN + VOL_AAD_LEN, hash);
	if (r != OSK_OK) {
		return r;
	}
	if (!osk_crypto_equal(hash, leaf, OSK_VOL_LEAF_LEN)) {
		return OSK_ERR_FORMAT;
	}
	memcpy(out, ct_buf, OSK_VOL_BLOCK_LEN);
	return OSK_OK;
}

osk_result_t osk_volume_commit(void)
{
	osk_result_t r;
	uint8_t next;

	if (!v.open) {
		return OSK_ERR_DENIED;
	}
	/* 1-3: Bloecke und Stempelseiten stehen schon (block_write), jetzt der
	 * Baum von unten nach oben. */
	r = tree_rebuild();
	if (r != OSK_OK) {
		return r;
	}
	v.version++;
	if (v.mode == OSK_VOL_MODE_DEVICE) {
		uint64_t c = 0u;

		r = osk_se050_counter_read(v.cfg.counter_id, &c);
		if (r != OSK_OK) {
			return r;
		}
		v.counter_target = c + 1u;
	}
	/* 4: in den INAKTIVEN Satz. Bricht es hier ab, gilt der alte. */
	next = (uint8_t)(v.active ^ 1u);
	root_serialize(root_buf);
	r = root_seal(root_buf);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_card_write(v.lay.root_page[next], root_buf, OSK_VOL_BLOCK_LEN);
	if (r != OSK_OK) {
		return r;
	}
	r = osk_card_sync();
	if (r != OSK_OK) {
		return r;
	}
	/* 5: der Festschreibepunkt. In Fall A der Zaehlerschritt im Chip, in
	 * Fall B der signierte Satz selbst - dort gibt es dieses Fenster
	 * nicht. */
	if (v.mode == OSK_VOL_MODE_DEVICE) {
		r = osk_se050_counter_raise(v.cfg.counter_id, v.counter_target);
		if (r != OSK_OK) {
			return r;
		}
	}
	v.active = next;
	v.dirty = 0u;
	return OSK_OK;
}

osk_result_t osk_volume_seal(void)
{
	if (!v.open) {
		return OSK_ERR_DENIED;
	}
	if (v.mode != OSK_VOL_MODE_SEALED) {
		/* Versiegeln ist der zweite Schritt von Modus 3. In Modus 1
		 * und 2 gibt es ihn nicht - dort ist der Schreibschutz nicht
		 * die Zusage, die der Nutzer braucht. */
		return OSK_ERR_UNSUPPORTED;
	}
	v.sealed = true;
	v.write_enabled = false;
	return osk_volume_commit();
}

osk_result_t osk_volume_full_check_range(uint32_t from, uint32_t count, uint32_t *bad_block_out)
{
	uint32_t i;

	if (!v.open) {
		return OSK_ERR_DENIED;
	}
	for (i = from; i < from + count && i < v.lay.data_blocks; i++) {
		osk_result_t r = osk_volume_block_read(i, scratch);

		if (r != OSK_OK) {
			if (bad_block_out != NULL) {
				*bad_block_out = i;
			}
			return r;
		}
	}
	return OSK_OK;
}
