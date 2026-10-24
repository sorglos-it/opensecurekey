/* Der oeffentliche Teil eines Eintrags - ohne Freigabe, ohne das Geheimnis.
 *
 * Warum es diese Funktion gibt, steht in osk_store.h. Hier wird geprueft, was
 * sie zusichert, und zwar so, dass die Zusicherung nicht bloss behauptet ist:
 *
 *  1. Sie fragt NICHT am Geraet nach Freigabe. Die Anzeige-Attrappe zaehlt
 *     jede Frage mit, also wird nachgezaehlt.
 *  2. Sie liest den Datenplatz des Geheimnisses NICHT. Die Flash-Attrappe
 *     zaehlt Lesezugriffe je Sektor, also wird auch das nachgezaehlt - und
 *     zwar genau fuer die zwei Sektoren dieses einen Platzes.
 *  3. Bei einem Typ ohne oeffentlichen Teil kommt OSK_ERR_UNSUPPORTED und
 *     kein stillschweigend leerer Puffer.
 *
 * Punkt 2 ist der eigentliche Beleg. Er faellt nur dann, wenn der Weg wirklich
 * einen eigenen Platz liest - genau darum liegt der oeffentliche Teil nicht
 * beim Geheimnis. */

#include <stdio.h>
#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_flash.h"
#include "fake_crypto.h"
#include "fake_se050.h"
#include "fake_ui.h"
#include "store_internal.h"
#include "osk_store_profiles.h"
#include "osk_se050.h"

/* Ein oeffentlicher Schluessel, wie ihn ssh-add ausgeben wuerde - Text, damit
 * sich im Flash nachsehen laesst, ob er im Klartext herumliegt.
 *
 * Absichtlich Variablen und keine #define: die Pruefmakros setzen den
 * Aufruftext in die Fehlermeldung, und dabei wird ein Makro ueber zwei Ebenen
 * expandiert. Mit #define stand der ganze Schluessel zweimal in einer Meldung
 * von 200 Byte - gcc 14.2 bricht darueber ab (-Wformat-truncation), clang
 * nicht. Ein Bezeichner wird nicht expandiert und bleibt kurz. */
static const char PUB1[] = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIOeffentlichEins thomas@stick";
static const char PUB2[] = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIOeffentlichZwei thomas@stick";
static const char GEHEIM1[] = "-----BEGIN OPENSSH PRIVATE KEY-----geheim-eins-----END-----";

/* Kurzer Aufruf, damit die Meldung der Pruefmakros kurz bleibt. */
static osk_result_t set_pub(osk_item_id_t id, const char *pub)
{
	return osk_store_item_set_public(id, (const uint8_t *)pub,
					 (pub != NULL) ? strlen(pub) : 0u);
}

/* Legt einen Eintrag eines bestimmten Typs an. */
static osk_result_t add_typed(const char *name, osk_item_type_t type, const char *secret,
			      osk_item_id_t *id_out)
{
	osk_item_meta_t meta;

	memset(&meta, 0, sizeof(meta));
	meta.type = type;
	meta.profile = 0;
	meta.name = name;
	meta.site = "";
	return osk_store_add(&meta, (const uint8_t *)secret, strlen(secret), id_out);
}

/* Die zwei Sektoren, auf denen das Geheimnis dieses Eintrags liegt. */
static void secret_area(osk_item_id_t id, uint32_t *off, uint32_t *len)
{
	uint8_t profile = 0;
	size_t index = 0;
	osk_idx_rec_t *rec = NULL;

	*off = 0;
	*len = 0;
	/* Ueber den Index, nicht geraten: der Platz kann jeder sein. */
	for (size_t i = 0; i < osk_index_count(0); i++) {
		if (osk_index_rec(0, i, &rec) != OSK_OK) {
			return;
		}
		if (rec->id == id) {
			*off = osk_blob_addr(rec->slot);
			*len = OSK_BLOB_SLOT;
			return;
		}
	}
	(void)profile;
	(void)index;
}

/* ---- Der Tresor haelt den oeffentlichen Teil -------------------------- */

static void case_from_vault(void)
{
	osk_item_id_t id = 0;
	uint8_t out[OSK_PUBLIC_MAX];
	size_t len = 0;

	osk_test_case("Ohne hinterlegten oeffentlichen Teil: nichts gefunden");
	t_device(0, 1u, T_PIN);
	T_OK(add_typed("Arbeitsschluessel", OSK_ITEM_SSH_KEY, GEHEIM1, &id));
	T_RES(osk_store_public(id, out, sizeof(out), &len), OSK_ERR_NOT_FOUND);
	T_EQ(len, 0);

	osk_test_case("Hinterlegt und wieder herausgegeben");
	T_OK(set_pub(id, PUB1));
	T_OK(osk_store_public(id, out, sizeof(out), &len));
	T_EQ(len, strlen(PUB1));
	T_MEM(out, PUB1, strlen(PUB1));

	osk_test_case("Nach dem Neustart auch");
	t_reboot();
	len = 0;
	T_OK(osk_store_public(id, out, sizeof(out), &len));
	T_EQ(len, strlen(PUB1));
	T_MEM(out, PUB1, strlen(PUB1));

	osk_test_case("Das Geheimnis liegt unberuehrt daneben");
	T_TRUE(t_secret_is(id, GEHEIM1));

	osk_test_case("Ueberschreiben ersetzt ihn, nicht ergaenzt");
	T_OK(set_pub(id, PUB2));
	T_OK(osk_store_public(id, out, sizeof(out), &len));
	T_EQ(len, strlen(PUB2));
	T_MEM(out, PUB2, strlen(PUB2));

	osk_test_case("Laenge 0 nimmt ihn wieder weg");
	T_OK(set_pub(id, NULL));
	T_RES(osk_store_public(id, out, sizeof(out), &len), OSK_ERR_NOT_FOUND);
	t_reboot();
	T_RES(osk_store_public(id, out, sizeof(out), &len), OSK_ERR_NOT_FOUND);
	T_TRUE(t_secret_is(id, GEHEIM1));
}

/* ---- Die beiden Zusicherungen, nachgezaehlt --------------------------- */

static void case_no_confirm_no_secret(void)
{
	osk_item_id_t id = 0;
	uint8_t out[OSK_PUBLIC_MAX];
	size_t len = 0;
	uint32_t off = 0, area = 0;

	osk_test_case("Es wird nie nach Freigabe gefragt");
	t_device(0, 1u, T_PIN);
	T_OK(add_typed("Arbeitsschluessel", OSK_ITEM_SSH_KEY, GEHEIM1, &id));
	T_OK(set_pub(id, PUB1));

	/* Die Attrappe wuerde JA sagen - wenn sie gefragt wuerde. Genau darum
	 * ist eine Null hier ein Beleg und nicht nur ein ausgebliebener
	 * Fehler. */
	fake_ui_answer(true);
	fake_ui_reset();
	for (int i = 0; i < 5; i++) {
		T_OK(osk_store_public(id, out, sizeof(out), &len));
	}
	T_EQ(fake_ui_asks(), 0);

	osk_test_case("Und der Datenplatz des Geheimnisses wird nicht gelesen");
	secret_area(id, &off, &area);
	T_TRUE(area == OSK_BLOB_SLOT);

	/* Erst die Gegenprobe: dass der Zaehler ueberhaupt anschlaegt. Ohne
	 * sie misst eine Null nur, dass niemand hinsieht. */
	fake_flash_read_reset();
	T_TRUE(t_secret_is(id, GEHEIM1));
	uint32_t beim_lesen = fake_flash_reads_in(off, area);

	T_TRUE(beim_lesen > 0u);

	/* Und jetzt die Zusicherung selbst. */
	fake_flash_read_reset();
	for (int i = 0; i < 5; i++) {
		T_OK(osk_store_public(id, out, sizeof(out), &len));
	}
	uint32_t beim_oeffentlichen = fake_flash_reads_in(off, area);

	printf("      Lesezugriffe auf den Platz des Geheimnisses: %u beim Herausgeben, "
	       "%u bei fuenf Abfragen des oeffentlichen Teils\n",
	       beim_lesen, beim_oeffentlichen);
	T_EQ(beim_oeffentlichen, 0);

	/* Gelesen wurde aber etwas - sonst waere die Null oben nur die Null
	 * eines Weges, der gar nichts tut. */
	osk_test_case("Gelesen wurde dabei der eigene Platz");
	T_TRUE(fake_flash_reads_in(OSK_AREA_BLOB_OFF, OSK_AREA_BLOB_LEN) +
		   fake_flash_reads_in(OSK_AREA_BLOB2_OFF, OSK_AREA_BLOB2_LEN) >
	       0u);
}

/* ---- Typen ohne oeffentlichen Teil ------------------------------------ */

static void case_wrong_type(void)
{
	uint8_t out[OSK_PUBLIC_MAX];
	size_t len = 0;

	osk_test_case("Passwort, Notiz und TOTP haben keinen oeffentlichen Teil");
	t_device(0, 1u, T_PIN);

	const osk_item_type_t ohne[3] = {OSK_ITEM_LOGIN, OSK_ITEM_NOTE, OSK_ITEM_TOTP};
	const char *namen[3] = {"Zugang", "Zettel", "Einmalcode"};

	for (int i = 0; i < 3; i++) {
		osk_item_id_t id = 0;

		T_OK(add_typed(namen[i], ohne[i], "irgendwas", &id));

		/* Der Puffer wird vorher gefuellt: eine Funktion, die
		 * stillschweigend nichts tut, wuerde ihn unberuehrt lassen und
		 * OSK_OK melden - das soll auffallen. */
		memset(out, 'X', sizeof(out));
		len = 4242u;
		T_RES(osk_store_public(id, out, sizeof(out), &len), OSK_ERR_UNSUPPORTED);
		T_EQ(len, 0);
		T_EQ(out[0], 'X');

		/* Hinterlegen laesst sich bei diesen Typen auch nichts. */
		T_RES(set_pub(id, PUB1), OSK_ERR_UNSUPPORTED);
	}

	osk_test_case("Zertifikat und PGP-Schluessel dagegen schon");
	const osk_item_type_t mit[2] = {OSK_ITEM_CERT, OSK_ITEM_PGP_KEY};

	for (int i = 0; i < 2; i++) {
		osk_item_id_t id = 0;

		T_OK(add_typed("Papier", mit[i], "geheim", &id));
		T_OK(set_pub(id, PUB1));
		T_OK(osk_store_public(id, out, sizeof(out), &len));
		T_EQ(len, strlen(PUB1));
	}
}

/* ---- Der Schluessel liegt im Chip ------------------------------------- */

static void case_from_se050(void)
{
	osk_item_id_t id = 0;
	uint8_t out[OSK_PUBLIC_MAX];
	uint8_t vom_chip[OSK_PUBLIC_MAX];
	size_t len = 0, chip_len = 0;
	const osk_key_id_t KEY = 0x0000F310u;

	osk_test_case("Liegt der Schluessel im Chip, kommt der Teil von dort");
	t_device(0, 1u, T_PIN);

	osk_key_policy_t policy;

	memset(&policy, 0, sizeof(policy));
	T_OK(osk_se050_key_create(KEY, OSK_CURVE_P256, &policy));
	T_OK(osk_se050_key_public(KEY, vom_chip, sizeof(vom_chip), &chip_len));
	T_TRUE(chip_len > 0u);

	/* Ein Eintrag ohne Geheimnis im Tresor gibt es nicht - das Geheimnis
	 * ist hier nur ein Platzhalter, der private Schluessel bleibt im
	 * Chip. */
	T_OK(add_typed("Chip-Schluessel", OSK_ITEM_SSH_KEY, "platzhalter", &id));
	T_OK(osk_store_item_set_se050_key(id, KEY));

	T_OK(osk_store_public(id, out, sizeof(out), &len));
	T_EQ(len, chip_len);
	T_MEM(out, vom_chip, chip_len);

	osk_test_case("Auch dabei keine Freigabe und kein Geheimnis");
	uint32_t off = 0, area = 0;

	secret_area(id, &off, &area);
	fake_ui_answer(true);
	fake_ui_reset();
	fake_flash_read_reset();
	T_OK(osk_store_public(id, out, sizeof(out), &len));
	T_EQ(fake_ui_asks(), 0);
	T_EQ(fake_flash_reads_in(off, area), 0);

	osk_test_case("Nach dem Neustart steht der Vermerk noch");
	t_reboot();
	len = 0;
	T_OK(osk_store_public(id, out, sizeof(out), &len));
	T_EQ(len, chip_len);

	osk_test_case("Vermerk weg: wieder nichts gefunden");
	T_OK(osk_store_item_set_se050_key(id, 0u));
	T_RES(osk_store_public(id, out, sizeof(out), &len), OSK_ERR_NOT_FOUND);
}

/* ---- Was der Nachbar nicht sehen darf --------------------------------- */

static void case_not_plaintext(void)
{
	osk_item_id_t id = 0;

	osk_test_case("Im Flash steht der oeffentliche Teil nicht im Klartext");
	t_device(0, 1u, T_PIN);
	T_OK(add_typed("Arbeitsschluessel", OSK_ITEM_SSH_KEY, GEHEIM1, &id));
	T_OK(set_pub(id, PUB1));

	/* Nicht geheim heisst nicht, dass jeder, der den Flash ausliest, die
	 * Liste der Schluessel des Nutzers bekommen soll - genau dasselbe
	 * Argument wie bei der Kennung zum Suchen. */
	static uint8_t bereich[OSK_AREA_BLOB_LEN];

	T_OK(osk_flash_read(OSK_AREA_BLOB_OFF, bereich, sizeof(bereich)));
	T_TRUE(memmem_missing(bereich, sizeof(bereich), PUB1));
	T_TRUE(memmem_missing(bereich, sizeof(bereich), "thomas@stick"));

	osk_test_case("Ein gesperrtes Profil gibt ihn nicht heraus");
	uint8_t out[OSK_PUBLIC_MAX];
	size_t len = 0;

	osk_store_profile_lock(0);
	T_RES(osk_store_public(id, out, sizeof(out), &len), OSK_ERR_NOT_FOUND);
	T_EQ(len, 0);
}

/* ---- Ein neues Geheimnis nimmt ihn mit weg ---------------------------- */

static void case_new_secret_drops_public(void)
{
	osk_item_id_t id = 0;
	uint8_t out[OSK_PUBLIC_MAX];
	size_t len = 0;

	osk_test_case("Ein neues Geheimnis nimmt den oeffentlichen Teil mit weg");
	t_device(0, 1u, T_PIN);
	T_OK(add_typed("Arbeitsschluessel", OSK_ITEM_SSH_KEY, GEHEIM1, &id));
	T_OK(set_pub(id, PUB1));
	T_OK(osk_store_public(id, out, sizeof(out), &len));

	/* Ein anderer privater Schluessel - der alte oeffentliche Teil passt
	 * nicht mehr dazu, und ein falscher waere schlimmer als keiner: der
	 * ssh-Agent wuerde eine Identitaet anbieten, zu der er nicht
	 * unterschreiben kann. */
	T_OK(osk_store_update(id, NULL, (const uint8_t *)"anderer-privater-schluessel", 27));
	T_RES(osk_store_public(id, out, sizeof(out), &len), OSK_ERR_NOT_FOUND);
	t_reboot();
	T_RES(osk_store_public(id, out, sizeof(out), &len), OSK_ERR_NOT_FOUND);
	T_TRUE(t_secret_is(id, "anderer-privater-schluessel"));

	osk_test_case("Danach laesst er sich neu hinterlegen");
	T_OK(set_pub(id, PUB2));
	T_OK(osk_store_public(id, out, sizeof(out), &len));
	T_EQ(len, strlen(PUB2));
}

/* ---- Plaetze bleiben nicht liegen ------------------------------------- */

static size_t slots_used(void)
{
	size_t n = 0;

	for (uint32_t s = 0; s < OSK_BLOB_SLOTS; s++) {
		if (osk_space_slot_is_used(s)) {
			n++;
		}
	}
	return n;
}

static void case_slots_freed(void)
{
	osk_item_id_t id = 0;

	osk_test_case("Ein Eintrag mit oeffentlichem Teil belegt zwei Plaetze");
	t_device(0, 1u, T_PIN);

	size_t leer = slots_used();

	T_OK(add_typed("Arbeitsschluessel", OSK_ITEM_SSH_KEY, GEHEIM1, &id));
	T_EQ(slots_used(), leer + 1u);
	T_OK(set_pub(id, PUB1));
	T_EQ(slots_used(), leer + 2u);

	osk_test_case("Ueberschreiben belegt keinen dritten");
	T_OK(set_pub(id, PUB2));
	T_EQ(slots_used(), leer + 2u);

	osk_test_case("Loeschen gibt beide frei");
	T_OK(osk_store_delete(id));
	T_EQ(slots_used(), leer);
	t_reboot();
	T_EQ(slots_used(), leer);
}

/* ---- Stromausfall ----------------------------------------------------- */

static void case_power(void)
{
	uint32_t steps;
	osk_item_id_t id = 0;
	uint8_t out[OSK_PUBLIC_MAX];
	size_t len = 0;

	t_device(0, 1u, T_PIN);
	T_OK(add_typed("Arbeitsschluessel", OSK_ITEM_SSH_KEY, GEHEIM1, &id));
	fake_flash_count_reset();
	T_OK(set_pub(id, PUB1));
	steps = fake_flash_writes();

	osk_test_case("Hinterlegen: Ausfall an jeder Schreibstelle");
	for (uint32_t at = 1; at <= steps + 1u; at++) {
		t_device(0, 1u, T_PIN);
		T_OK(add_typed("Arbeitsschluessel", OSK_ITEM_SSH_KEY, GEHEIM1, &id));
		fake_crypto_nonce_reset();
		fake_flash_count_reset();
		fake_flash_fail_after(at);

		osk_result_t r = set_pub(id, PUB1);

		t_reboot();

		/* Entweder hinterlegt oder nicht - nie ein halber oder ein
		 * falscher oeffentlicher Teil. */
		osk_result_t got = osk_store_public(id, out, sizeof(out), &len);

		T_TRUE(got == OSK_OK || got == OSK_ERR_NOT_FOUND);
		if (got == OSK_OK) {
			T_EQ(len, strlen(PUB1));
			T_MEM(out, PUB1, strlen(PUB1));
		}
		if (r == OSK_OK) {
			T_RES(got, OSK_OK);
		}
		/* Und das Geheimnis ist in jedem Fall unversehrt. */
		T_TRUE(t_secret_is(id, GEHEIM1));
		T_EQ(fake_crypto_nonce_dups(), 0);
		T_EQ(fake_crypto_vault_nonce_dups(), 0);
	}
	printf("      %u Schreibstellen beim Hinterlegen geprueft\n", steps + 1u);
}

void test_public(void)
{
	case_from_vault();
	case_no_confirm_no_secret();
	case_wrong_type();
	case_from_se050();
	case_not_plaintext();
	case_new_secret_drops_public();
	case_slots_freed();
	case_power();
}
