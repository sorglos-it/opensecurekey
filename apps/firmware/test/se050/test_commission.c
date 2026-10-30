/* Der Erstlauf gegen den Nachbau.
 *
 * WAS DIESER TEST ANDERS MACHT ALS EINE ATTRAPPENPRUEFUNG: die Haken
 * `rotate`, `require_scp` und `probe` gehen NICHT an eine Attrappe, sondern
 * durch den echten Treiber auf den Nachbau - T=1', Applet, SCP03,
 * verschluesselter Befehl. Und der Nachbau packt PUT KEY wirklich aus: er
 * entschluesselt mit dem Key-DEK, der bei IHM gilt, und prueft den
 * Pruefwert nach (se050_sim.h, sim_scp_sets_live). Ein Erstlauf, der mit
 * dem falschen Schluessel verpackt, wird hier abgelehnt statt freundlich
 * mit 0x9000 beantwortet.
 *
 * Das ist die Lehre aus dem 29.10.2026: eine Attrappe, die nur die eigene
 * Seite des Vertrags prueft, ist gruen und am Geraet waere nichts
 * geschehen. Darum prueft der Schlusstest hier nicht "der Aufruf kam
 * durch", sondern "der Chip hat jetzt GENAU die Schluessel, die im Zettel
 * stehen".
 *
 * Was trotzdem offen bleibt, steht in docs/erstlauf.md unter "Was am echten
 * Chip offen ist". Allen voran: ob der SE051 PUT KEY ueber diesen Kanal
 * ueberhaupt annimmt. */
#include "tiny_test.h"
#include "fake_bus.h"
#include "se050_sim.h"
#include "test_platform.h"
#include "osk_se050.h"
#include "osk_commission.h"

int boot_sim(fake_bus_t *fb, se050_sim_t *sim);

/* Werkssaetze fuer den Prueflauf - ERFUNDENE Werte.
 *
 * Die echten stehen in NXP AN12973 Tabelle 6 und haengen am
 * Fertigungslos. Sie gehoeren nicht in dieses Repo: der Treiber braucht
 * sie nie fest eingebaut, die Platinenschicht liefert sie. Fuer den Test
 * reicht, dass es zwei verschiedene bekannte Saetze sind - geprueft wird
 * der Ablauf, nicht der Zahlenwert. */
static const osk_scp_keyset_t FAC_MAIN = {
    .enc = {0xF1, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
	    0x0E, 0x0F},
    .mac = {0xF2, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
	    0x1E, 0x1F},
    .dek = {0xF3, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D,
	    0x2E, 0x2F},
};
static const osk_scp_keyset_t FAC_RESCUE = {
    .enc = {0xE1, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,
	    0x3E, 0x3F},
    .mac = {0xE2, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
	    0x4E, 0x4F},
    .dek = {0xE3, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
	    0x5E, 0x5F},
};

/* --- Die Umgebung des Erstlaufs ------------------------------------------
 *
 * Flash, Zufall und die zwei Stellen, an denen sich ein Stromverlust
 * erzwingen laesst. Ein Abbruch, den man nicht erzwingen kann, ist keine
 * Pruefung - darum sind es Zaehler und keine Flaggen: "der dritte
 * Schreibvorgang fehlt" ist eine Stelle im Ablauf, nicht ein Gefuehl. */
typedef struct {
	/* Der Zettel im Flash. */
	osk_commission_record_t rec;
	bool have;
	size_t stores;
	size_t store_fail_at; /* 0 = nie; sonst schlaegt der n-te Schreibvorgang fehl */

	size_t rotates;
	size_t rotate_fail_at; /* Abbruch, BEVOR der Befehl den Chip erreicht */
	uint8_t rotate_order[4];
	size_t rotate_order_len;

	bool require_fail;

	/* Zufallsquellen. zero_mcu / zero_chip stellen eine ausgefallene
	 * Quelle nach, die trotzdem Erfolg meldet. */
	uint8_t mcu_counter;
	uint8_t chip_counter;
	bool zero_mcu;
	bool zero_chip;
	bool fail_chip_random;

	se050_sim_t *sim;
	fake_bus_t *fb;
} fixture_t;

static fixture_t g_fx;
static const osk_commission_backend_t *g_be;

static osk_result_t fx_load(osk_commission_record_t *out)
{
	if (!g_fx.have) {
		return OSK_ERR_NOT_FOUND;
	}
	*out = g_fx.rec;
	return OSK_OK;
}

/* Schreiben ist ganz oder gar nicht - im Geraet macht das die
 * Tresorschicht mit zwei abwechselnden Abdruecken. Ein erzwungener Fehler
 * laesst also den ALTEN Zettel stehen, nicht einen halben. */
static osk_result_t fx_store(const osk_commission_record_t *rec)
{
	g_fx.stores++;
	if (g_fx.store_fail_at != 0 && g_fx.stores == g_fx.store_fail_at) {
		return OSK_ERR_IO;
	}
	g_fx.rec = *rec;
	g_fx.have = true;
	return OSK_OK;
}

static osk_result_t fx_random_mcu(uint8_t *out, size_t len)
{
	if (g_fx.zero_mcu) {
		memset(out, 0, len);
		return OSK_OK;
	}
	for (size_t i = 0; i < len; i++) {
		g_fx.mcu_counter = (uint8_t)(g_fx.mcu_counter + 0x6Du);
		out[i] = (uint8_t)(g_fx.mcu_counter ^ 0x5Au);
	}
	return OSK_OK;
}

/* Die Chipquelle ist hier eine Rechnung im Test, nicht osk_se050_random()
 * ueber den Bus. Was sie pruefen soll, ist die Regel "zwei Quellen, und
 * keine halbe" - nicht der Weg des Zufalls durch den Treiber; den prueft
 * test_contract.c. Dass es auf dem Geraet osk_se050_random() ist, steht im
 * Vertrag in osk_commission.h. */
static osk_result_t fx_random_chip(uint8_t *out, size_t len)
{
	if (g_fx.fail_chip_random) {
		return OSK_ERR_IO;
	}
	if (g_fx.zero_chip) {
		memset(out, 0, len);
		return OSK_OK;
	}
	for (size_t i = 0; i < len; i++) {
		g_fx.chip_counter = (uint8_t)(g_fx.chip_counter + 0x1Fu);
		out[i] = (uint8_t)(g_fx.chip_counter ^ 0xA5u);
	}
	return OSK_OK;
}

static osk_result_t fx_factory_keys(uint8_t key_version, osk_scp_keyset_t *out)
{
	if (key_version == OSK_SCP_SET_MAIN) {
		*out = FAC_MAIN;
		return OSK_OK;
	}
	if (key_version == OSK_SCP_SET_RESCUE) {
		*out = FAC_RESCUE;
		return OSK_OK;
	}
	return OSK_ERR_NOT_FOUND;
}

/* Welcher Satz 11 gilt gerade - aus dem Zettel, sonst der Werkssatz. Auf
 * dem Geraet steht genau das im SCP03-Haken der Platine. */
static void active_main(osk_scp_keyset_t *out)
{
	if (osk_commission_active_keys(g_be, OSK_SCP_SET_MAIN, out) != OSK_OK) {
		*out = FAC_MAIN;
	}
}

/* Den Kanal mit einem bestimmten Satz aufbauen und offen lassen. Das ist
 * die Frage an den Chip: gilt dieser Satz? */
static osk_result_t open_with(const osk_scp_keyset_t *keys)
{
	osk_scp03_static_keys_t host;

	memcpy(host.enc, keys->enc, OSK_SCP_KEY_LEN);
	memcpy(host.mac, keys->mac, OSK_SCP_KEY_LEN);
	memcpy(host.dek, keys->dek, OSK_SCP_KEY_LEN);

	osk_se050_close();
	fb_init(g_fx.fb);
	sim_reattach(g_fx.sim, g_fx.fb);
	tp_set_host_keys(&host);
	return boot_sim(g_fx.fb, g_fx.sim) != 0 ? OSK_OK : OSK_ERR_DENIED;
}

static osk_result_t fx_probe(const osk_scp_keyset_t *keys)
{
	osk_scp_keyset_t k;

	if (keys != NULL) {
		k = *keys;
	} else {
		active_main(&k);
	}
	const osk_result_t r = open_with(&k);
	if (r == OSK_OK) {
		osk_se050_close();
	}
	return r;
}

static osk_result_t fx_rotate(uint8_t key_version, const osk_scp_keyset_t *current,
			      const osk_scp_keyset_t *replacement)
{
	g_fx.rotates++;
	if (g_fx.rotate_order_len < sizeof(g_fx.rotate_order)) {
		g_fx.rotate_order[g_fx.rotate_order_len++] = key_version;
	}
	if (g_fx.rotate_fail_at != 0 && g_fx.rotates == g_fx.rotate_fail_at) {
		/* Abbruch, bevor der Befehl auf den Bus geht - Stecker
		 * gezogen, waehrend der Treiber noch baut. */
		return OSK_ERR_IO;
	}

	osk_scp_keyset_t act;
	active_main(&act);
	const osk_result_t r = open_with(&act);
	if (r != OSK_OK) {
		return r;
	}
	return osk_se050_rotate_scp_keys(key_version, current, replacement);
}

static osk_result_t fx_require_scp(void)
{
	if (g_fx.require_fail) {
		return OSK_ERR_IO;
	}

	osk_scp_keyset_t act;
	active_main(&act);
	const osk_result_t r = open_with(&act);
	if (r != OSK_OK) {
		return r;
	}
	return osk_se050_require_scp();
}

static const osk_commission_backend_t BE = {
    .load = fx_load,
    .store = fx_store,
    .random_mcu = fx_random_mcu,
    .random_chip = fx_random_chip,
    .factory_keys = fx_factory_keys,
    .probe = fx_probe,
    .rotate = fx_rotate,
    .require_scp = fx_require_scp,
};

/* Ein frisches Geraet: leerer Flash, Chip im Werkszustand. */
static void fresh(fake_bus_t *fb, se050_sim_t *sim)
{
	osk_se050_close();
	memset(&g_fx, 0, sizeof(g_fx));
	g_fx.sim = sim;
	g_fx.fb = fb;
	g_be = &BE;

	fb_init(fb);
	sim_attach(sim, fb);
	g_bus = fb;
	tp_set_host_keys(NULL);

	osk_scp03_static_keys_t fm;
	osk_scp03_static_keys_t fr;
	memcpy(fm.enc, FAC_MAIN.enc, OSK_SCP_KEY_LEN);
	memcpy(fm.mac, FAC_MAIN.mac, OSK_SCP_KEY_LEN);
	memcpy(fm.dek, FAC_MAIN.dek, OSK_SCP_KEY_LEN);
	memcpy(fr.enc, FAC_RESCUE.enc, OSK_SCP_KEY_LEN);
	memcpy(fr.mac, FAC_RESCUE.mac, OSK_SCP_KEY_LEN);
	memcpy(fr.dek, FAC_RESCUE.dek, OSK_SCP_KEY_LEN);
	sim_scp_sets_live(sim, &fm, &fr);
}

/* Haelt der Chip genau den Satz, der im Zettel steht? Das ist die Frage,
 * an der eine Attrappenpruefung vorbeigeht. */
static void chip_matches_record(se050_sim_t *sim, size_t slot, const char *what)
{
	const osk_scp_keyset_t *want = &g_fx.rec.keys[slot];

	TT_EQ_MEM(sim->scp_set[slot].enc, want->enc, OSK_SCP_KEY_LEN, what);
	TT_EQ_MEM(sim->scp_set[slot].mac, want->mac, OSK_SCP_KEY_LEN, what);
	TT_EQ_MEM(sim->scp_set[slot].dek, want->dek, OSK_SCP_KEY_LEN, what);
}

void test_commission(void)
{
	TT_CASE("Erstlauf: ein frisches Geraet tauscht beide Saetze");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);

		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "Erstlauf durch");

		TT_EQ_INT(g_fx.rec.stage, OSK_COMMISSION_DONE, "Zettel steht auf fertig");
		TT_EQ_INT(g_fx.rec.done, OSK_COMMISSION_DONE_BOTH, "beide Bits");
		TT_TRUE(sim.scp_replaced[0], "Chip hat Satz 11 ersetzt");
		TT_TRUE(sim.scp_replaced[1], "Chip hat Satz 12 ersetzt");
		TT_TRUE(sim.scp_required, "Kanal ist verpflichtend");
		TT_EQ_INT(sim.put_key_calls, 2u, "genau zwei PUT KEY - einer je Satz");
		TT_EQ_INT(sim.put_key_rejected, 0u, "keiner abgelehnt");

		/* Der eigentliche Beweis: der Chip traegt dieselben
		 * Schluessel, die das Geraet aufgeschrieben hat. Haette der
		 * Erstlauf mit dem falschen Key-DEK verpackt, haette der
		 * Nachbau abgelehnt - und waere der Nachbau eine Attrappe,
		 * stimmte hier nichts ueberein. */
		chip_matches_record(&sim, 0, "Satz 11 im Chip = Satz 11 im Zettel");
		chip_matches_record(&sim, 1, "Satz 12 im Chip = Satz 12 im Zettel");

		/* Und sie sind nicht die Werkssaetze. */
		TT_TRUE(memcmp(&g_fx.rec.keys[0], &FAC_MAIN, sizeof(FAC_MAIN)) != 0,
			"Satz 11 ist nicht der Werkssatz");
		TT_TRUE(memcmp(&g_fx.rec.keys[1], &FAC_RESCUE, sizeof(FAC_RESCUE)) != 0,
			"Satz 12 ist nicht der Werkssatz");
		TT_TRUE(memcmp(&g_fx.rec.keys[0], &g_fx.rec.keys[1], sizeof(FAC_MAIN)) != 0,
			"die zwei neuen Saetze sind verschieden");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: der Rettungssatz 12 kommt VOR dem Arbeitssatz 11");
	{
		/* Solange Satz 12 NXP gehoert, ist ein getauschter Satz 11
		 * nichts wert - er liesse sich damit neu setzen. */
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);

		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "Erstlauf durch");
		TT_EQ_INT(g_fx.rotate_order_len, 2u, "zwei Befehle");
		TT_EQ_INT(g_fx.rotate_order[0], OSK_SCP_SET_RESCUE, "erst Satz 12");
		TT_EQ_INT(g_fx.rotate_order[1], OSK_SCP_SET_MAIN, "dann Satz 11");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: mit den Werksschluesseln kommt danach niemand hinein");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);
		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "Erstlauf durch");

		/* Kanal mit dem Werkssatz: der Nachbau lehnt ab, weil er
		 * den Satz wirklich ersetzt hat. */
		TT_TRUE(fx_probe(&FAC_MAIN) != OSK_OK, "Werkssatz 11 oeffnet nicht mehr");
		TT_TRUE(fx_probe(&FAC_RESCUE) != OSK_OK, "Werkssatz 12 oeffnet nicht mehr");

		/* Und ein PUT KEY, das mit dem Werks-Key-DEK verpackt ist,
		 * wird abgelehnt - das ist die Gegenprobe, dass der Nachbau
		 * wirklich auspackt und nicht nur 0x9000 sagt. */
		osk_scp_keyset_t act;
		active_main(&act);
		TT_EQ_INT(open_with(&act), OSK_OK, "mit dem eigenen Satz geht es");

		const size_t before = sim.put_key_rejected;
		TT_TRUE(osk_se050_rotate_scp_keys(OSK_SCP_SET_MAIN, &FAC_MAIN, &FAC_RESCUE) !=
			    OSK_OK,
			"Tausch mit dem Werks-Key-DEK scheitert");
		TT_EQ_INT(sim.put_key_rejected, before + 1u, "und der Nachbau hat abgelehnt");
		chip_matches_record(&sim, 0, "Satz 11 im Chip ist noch der eigene");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: ein zweiter Lauf schickt keinen einzigen Befehl");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);
		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "erster Lauf");

		const size_t puts = sim.put_key_calls;
		const size_t stores = g_fx.stores;
		const size_t rots = g_fx.rotates;

		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "zweiter Lauf meldet fertig");
		TT_EQ_INT(sim.put_key_calls, puts, "kein weiteres PUT KEY");
		TT_EQ_INT(g_fx.rotates, rots, "kein weiterer Tausch");
		TT_EQ_INT(g_fx.stores, stores, "und nichts neu geschrieben");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: Abbruch vor dem ersten Befehl - Geraet bleibt ansprechbar");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);
		g_fx.rotate_fail_at = 1u; /* Satz 12 geht nicht hinaus */

		TT_TRUE(osk_commission_run(&BE) != OSK_OK, "Lauf gescheitert");
		TT_EQ_INT(g_fx.rec.stage, OSK_COMMISSION_PENDING, "Zettel steht auf laufend");
		TT_EQ_INT(g_fx.rec.done, 0u, "kein Satz getauscht");
		TT_EQ_INT(sim.put_key_calls, 0u, "der Chip hat nichts bekommen");

		/* Ansprechbar: der Werkssatz gilt noch, und der Zettel sagt
		 * genau das (active_keys findet nichts). */
		osk_scp_keyset_t k;
		TT_EQ_INT(osk_commission_active_keys(&BE, OSK_SCP_SET_MAIN, &k), OSK_ERR_NOT_FOUND,
			  "noch kein eigener Satz 11");
		TT_EQ_INT(fx_probe(NULL), OSK_OK, "Chip antwortet");

		/* Und der naechste Start macht fertig - mit DENSELBEN
		 * Schluesseln, nicht mit neuen. */
		osk_scp_keyset_t pinned_main = g_fx.rec.keys[0];
		osk_scp_keyset_t pinned_rescue = g_fx.rec.keys[1];
		g_fx.rotate_fail_at = 0;
		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "zweiter Start macht fertig");
		TT_EQ_MEM(&g_fx.rec.keys[0], &pinned_main, sizeof(pinned_main),
			  "Satz 11 unveraendert festgenagelt");
		TT_EQ_MEM(&g_fx.rec.keys[1], &pinned_rescue, sizeof(pinned_rescue),
			  "Satz 12 unveraendert festgenagelt");
		TT_EQ_INT(sim.put_key_calls, 2u, "genau zwei PUT KEY insgesamt");
		chip_matches_record(&sim, 0, "Chip hat den festgenagelten Satz 11");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: Abbruch zwischen Satz 12 und Satz 11");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);
		g_fx.rotate_fail_at = 2u; /* Satz 12 ist durch, Satz 11 nicht */

		TT_TRUE(osk_commission_run(&BE) != OSK_OK, "Lauf gescheitert");
		TT_EQ_INT(g_fx.rec.done, OSK_COMMISSION_DONE_RESCUE, "nur Satz 12 getauscht");
		TT_TRUE(sim.scp_replaced[1], "Chip hat Satz 12");
		TT_TRUE(!sim.scp_replaced[0], "Chip hat Satz 11 noch nicht");
		TT_TRUE(!sim.scp_required, "der Kanal ist NICHT verpflichtend gemacht");

		/* Ansprechbar: Satz 11 ist noch der Werkssatz, und genau den
		 * gibt der Zettel aus. */
		osk_scp_keyset_t k;
		TT_EQ_INT(osk_commission_active_keys(&BE, OSK_SCP_SET_MAIN, &k), OSK_ERR_NOT_FOUND,
			  "Satz 11 noch Werkssatz");
		TT_EQ_INT(osk_commission_active_keys(&BE, OSK_SCP_SET_RESCUE, &k), OSK_OK,
			  "Satz 12 schon eigener");
		TT_EQ_INT(fx_probe(NULL), OSK_OK, "Chip antwortet");

		g_fx.rotate_fail_at = 0;
		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "naechster Start macht fertig");
		TT_EQ_INT(sim.put_key_calls, 2u, "Satz 12 wurde NICHT zweimal getauscht");
		TT_TRUE(sim.scp_required, "jetzt ist der Kanal verpflichtend");
		chip_matches_record(&sim, 0, "Satz 11 stimmt");
		chip_matches_record(&sim, 1, "Satz 12 stimmt");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: Satz 11 ist im Chip, der Zettel kam nicht mehr dazu");
	{
		/* Das engste Fenster: PUT KEY ist durchgegangen, der Strom
		 * fiel vor dem Schreiben des Zettels aus. Ohne Abgleich mit
		 * dem Chip waere das Geraet hier fuer immer stecken - der
		 * Werkssatz gilt nicht mehr, und der Zettel wuesste es
		 * nicht. */
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);

		/* Schreibvorgaenge: 1 = Zettel mit den Schluesseln (PENDING),
		 * 2 = Bit fuer Satz 12, 3 = Bit fuer Satz 11. Der dritte
		 * faellt aus. */
		g_fx.store_fail_at = 3u;

		TT_TRUE(osk_commission_run(&BE) != OSK_OK, "Lauf gescheitert");
		TT_EQ_INT(g_fx.rec.done, OSK_COMMISSION_DONE_RESCUE,
			  "der Zettel weiss nur von Satz 12");
		TT_TRUE(sim.scp_replaced[0], "der Chip hat Satz 11 aber schon");

		/* Der naechste Start fragt den Chip, statt dem Zettel zu
		 * glauben - und findet es. */
		g_fx.store_fail_at = 0;
		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "naechster Start raeumt das auf");
		TT_EQ_INT(g_fx.rec.done, OSK_COMMISSION_DONE_BOTH, "beide Bits");
		TT_EQ_INT(sim.put_key_calls, 2u, "und KEIN dritter PUT KEY");
		TT_EQ_INT(sim.put_key_rejected, 0u, "nichts abgelehnt");
		TT_TRUE(sim.scp_required, "Kanal verpflichtend");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: Satz 12 im Chip, Zettel fehlt - ehrlich stecken, aber ansprechbar");
	{
		/* Dasselbe Fenster am Rettungssatz. Hier kann der Abgleich
		 * NICHT helfen: dieser Nachbau baut Sitzungen nur gegen
		 * Satz 11 auf, also beantwortet der Chip die Frage "gilt
		 * mein Satz 12?" nicht. Der Lauf raet dann nicht, sondern
		 * versucht den Tausch - und der Nachbau lehnt ihn ab, weil
		 * der Werks-Key-DEK nicht mehr gilt.
		 *
		 * Das Ergebnis ist ein Geraet, das ANSPRECHBAR bleibt
		 * (Satz 11 ist unberuehrt) und dessen Erstlauf nicht fertig
		 * wird. Genau so steht es in docs/erstlauf.md, Frage 3 - es
		 * wird gemeldet und nicht uebergangen. */
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);
		g_fx.store_fail_at = 2u; /* Bit fuer Satz 12 kommt nicht an */

		TT_TRUE(osk_commission_run(&BE) != OSK_OK, "Lauf gescheitert");
		TT_TRUE(sim.scp_replaced[1], "Chip hat Satz 12");
		TT_EQ_INT(g_fx.rec.done, 0u, "der Zettel weiss nichts davon");

		g_fx.store_fail_at = 0;
		TT_TRUE(osk_commission_run(&BE) != OSK_OK, "naechster Start wird nicht fertig");
		TT_TRUE(sim.put_key_rejected > 0u, "der Nachbau hat den Tausch abgelehnt");
		TT_TRUE(!sim.scp_required, "und der Kanal wurde NICHT verpflichtend gemacht");
		TT_TRUE(!sim.scp_replaced[0], "Satz 11 ist unberuehrt");
		TT_EQ_INT(fx_probe(&FAC_MAIN), OSK_OK, "das Geraet ist ansprechbar");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: ein Baustein, der den Tausch nur behauptet, wird erwischt");
	{
		/* Die Falle vom 29.10.2026 von der anderen Seite: der
		 * Baustein antwortet auf jedes PUT KEY mit 0x9000 und
		 * tauscht nichts. Wer dem Statuswort glaubt, macht jetzt den
		 * Kanal verpflichtend - mit den Schluesseln aus AN12973 im
		 * Chip. Darum wird am Ende NACHGEMESSEN, ob die
		 * Werksschluessel wirklich weg sind. */
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);
		sim.put_key_lies = true;

		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_INTERNAL, "der Lauf merkt es");
		TT_EQ_INT(sim.put_key_calls, 2u, "beide Befehle gingen hinaus");
		TT_TRUE(!sim.scp_replaced[0] && !sim.scp_replaced[1], "getauscht wurde nichts");
		TT_TRUE(!sim.scp_required,
			"und der Kanal wurde NICHT verpflichtend gemacht");
		TT_TRUE(g_fx.rec.stage != OSK_COMMISSION_DONE, "der Zettel sagt nicht fertig");
		TT_EQ_INT(fx_probe(&FAC_MAIN), OSK_OK,
			  "der Werkssatz oeffnet noch - genau das war der Befund");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: require_scp scheitert - der Zettel bleibt auf laufend");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);
		g_fx.require_fail = true;

		TT_TRUE(osk_commission_run(&BE) != OSK_OK, "Lauf gescheitert");
		TT_EQ_INT(g_fx.rec.stage, OSK_COMMISSION_PENDING, "nicht auf fertig");
		TT_EQ_INT(g_fx.rec.done, OSK_COMMISSION_DONE_BOTH, "beide Saetze sind aber getauscht");
		TT_TRUE(!sim.scp_required, "der Kanal ist nicht verpflichtend");

		g_fx.require_fail = false;
		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "naechster Start holt es nach");
		TT_TRUE(sim.scp_required, "jetzt verpflichtend");
		TT_EQ_INT(sim.put_key_calls, 2u, "ohne einen dritten PUT KEY");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: ein zurueckgedrehter Zettel bekommt keinen zweiten ersten Start");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);
		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "Erstlauf durch");
		const size_t puts = sim.put_key_calls;

		/* Angreifer loescht den Flash. Der Chip behaelt seine
		 * Schluessel - und damit ist die Luege sichtbar. */
		g_fx.have = false;
		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_ROLLBACK, "kein Erstlauf");
		TT_EQ_INT(sim.put_key_calls, puts, "kein PUT KEY");

		/* Angreifer faelscht den Zettel auf Werkszustand. */
		g_fx.have = true;
		g_fx.rec.stage = (uint8_t)OSK_COMMISSION_FACTORY;
		g_fx.rec.done = 0u;
		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_ROLLBACK, "auch so nicht");
		TT_EQ_INT(sim.put_key_calls, puts, "kein PUT KEY");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: Zettel sagt fertig, Chip ist im Werkszustand");
	{
		/* Der Chip wurde zurueckgesetzt oder ausgetauscht. Ein
		 * Erstlauf waere jetzt genau das, was ein Angreifer will. */
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);

		g_fx.have = true;
		memset(&g_fx.rec, 0, sizeof(g_fx.rec));
		g_fx.rec.magic = OSK_COMMISSION_MAGIC;
		g_fx.rec.stage = (uint8_t)OSK_COMMISSION_DONE;
		g_fx.rec.done = OSK_COMMISSION_DONE_BOTH;
		memset(&g_fx.rec.keys[0], 0x77, sizeof(g_fx.rec.keys[0]));
		memset(&g_fx.rec.keys[1], 0x88, sizeof(g_fx.rec.keys[1]));

		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_ROLLBACK, "gemeldet, nicht getauscht");
		TT_EQ_INT(sim.put_key_calls, 0u, "kein PUT KEY");
		TT_TRUE(!sim.scp_required, "und nichts verpflichtend gemacht");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: eine ausgefallene Zufallsquelle bricht ab, bevor etwas geschieht");
	{
		fake_bus_t fb;
		se050_sim_t sim;

		/* Quelle im Rechenkern liefert Nullen und meldet Erfolg. */
		fresh(&fb, &sim);
		g_fx.zero_mcu = true;
		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_INTERNAL, "abgebrochen");
		TT_EQ_INT(g_fx.stores, 0u, "nichts aufgeschrieben");
		TT_EQ_INT(sim.put_key_calls, 0u, "nichts getauscht");

		/* Quelle im Chip liefert Nullen. */
		fresh(&fb, &sim);
		g_fx.zero_chip = true;
		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_INTERNAL, "abgebrochen");
		TT_EQ_INT(g_fx.stores, 0u, "nichts aufgeschrieben");

		/* Quelle im Chip antwortet gar nicht. */
		fresh(&fb, &sim);
		g_fx.fail_chip_random = true;
		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_IO, "Fehler weitergegeben");
		TT_EQ_INT(g_fx.stores, 0u, "nichts aufgeschrieben");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: ein unlesbarer Zettel wird gemeldet, nicht benutzt");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);

		g_fx.have = true;
		memset(&g_fx.rec, 0, sizeof(g_fx.rec));
		g_fx.rec.magic = 0xDEADBEEFu;
		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_FORMAT, "Format gemeldet");
		TT_EQ_INT(sim.put_key_calls, 0u, "nichts getauscht");

		g_fx.rec.magic = OSK_COMMISSION_MAGIC;
		g_fx.rec.stage = 9u;
		TT_EQ_INT(osk_commission_run(&BE), OSK_ERR_FORMAT, "unbekannte Stufe gemeldet");

		osk_se050_close();
	}

	TT_CASE("Erstlauf: ohne vollstaendige Haken laeuft nichts");
	{
		TT_EQ_INT(osk_commission_run(NULL), OSK_ERR_ARG, "kein Haken");

		osk_commission_backend_t be = BE;
		be.rotate = NULL;
		TT_EQ_INT(osk_commission_run(&be), OSK_ERR_ARG, "ohne rotate");

		be = BE;
		be.require_scp = NULL;
		TT_EQ_INT(osk_commission_run(&be), OSK_ERR_ARG, "ohne require_scp");

		be = BE;
		be.random_chip = NULL;
		TT_EQ_INT(osk_commission_run(&be), OSK_ERR_ARG, "ohne Chip-Zufall");

		be = BE;
		be.probe = NULL;
		TT_EQ_INT(osk_commission_run(&be), OSK_ERR_ARG, "ohne Frage an den Chip");

		be = BE;
		be.store = NULL;
		TT_EQ_INT(osk_commission_run(&be), OSK_ERR_ARG, "ohne Zettel");
	}

	TT_CASE("Erstlauf: active_keys nennt je Satz den Schluessel, der gilt");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		fresh(&fb, &sim);

		osk_scp_keyset_t k;
		TT_EQ_INT(osk_commission_active_keys(&BE, OSK_SCP_SET_MAIN, &k), OSK_ERR_NOT_FOUND,
			  "vor dem Erstlauf: Werkssatz");
		TT_EQ_INT(osk_commission_active_keys(&BE, 0x0Du, &k), OSK_ERR_ARG,
			  "ein Satz, den es nicht gibt");
		TT_EQ_INT(osk_commission_active_keys(&BE, OSK_SCP_SET_MAIN, NULL), OSK_ERR_ARG,
			  "ohne Ziel");

		TT_EQ_INT(osk_commission_run(&BE), OSK_OK, "Erstlauf durch");
		TT_EQ_INT(osk_commission_active_keys(&BE, OSK_SCP_SET_MAIN, &k), OSK_OK,
			  "danach: eigener Satz 11");
		TT_EQ_MEM(&k, &g_fx.rec.keys[0], sizeof(k), "und zwar der aus dem Zettel");
		TT_EQ_INT(osk_commission_active_keys(&BE, OSK_SCP_SET_RESCUE, &k), OSK_OK,
			  "und Satz 12");
		TT_EQ_MEM(&k, &g_fx.rec.keys[1], sizeof(k), "auch der aus dem Zettel");

		osk_se050_close();
	}

	/* Aufraeumen: die folgenden Pruefdateien erwarten die Vorgabe-
	 * Schluessel im Platinenhaken. */
	tp_set_host_keys(NULL);
	osk_se050_close();
}
