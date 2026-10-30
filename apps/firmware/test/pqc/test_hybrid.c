/* Die Zusammenfuehrung: ML-KEM-768 zusammen mit X25519.
 *
 * Hier wird nicht gerechnet, hier wird eine Regel geprueft: "hybrid oder
 * gar nicht". Die Faelle unten sind so gewaehlt, dass sie durchfallen,
 * wenn jemand spaeter eine Abkuerzung einbaut - einen Schalter, einen
 * Zweig fuer fehlenden X25519-Anteil, eine Mischung, in der ein Anteil
 * nicht mehr eingeht.
 *
 * Was hier NICHT geprueft wird, und zwar weil es nicht in diesem Modul
 * liegt: X25519 selbst. Der geheime Punkt steckt im Sicherheitschip, und
 * das rohe ECDH-Ergebnis kommt von dort. Hier stehen an seiner Stelle
 * feste 32 Byte. Ob der SE051 das rohe Ergebnis ueberhaupt herausgibt,
 * ist eine offene Frage am Geraet - siehe docs/pqc.md.
 */
#include "tiny_test.h"
#include "osk_pqc.h"
#include "pqc_keccak.h"

/* Zahlen aus dem Nichts, aber immer dieselben. */
static void seedbuf(uint8_t *out, size_t len, uint8_t tag)
{
	osk_shake256(out, len, &tag, 1u);
}

struct party {
	uint8_t x_pub[OSK_PQC_X_BYTES];
	uint8_t ek[OSK_PQC_EK_BYTES];
	uint8_t dk[OSK_PQC_DK_BYTES];
};

static void make_party(struct party *p, uint8_t tag)
{
	uint8_t d[32], z[32];

	seedbuf(p->x_pub, sizeof(p->x_pub), (uint8_t)(tag + 1u));
	seedbuf(d, sizeof(d), (uint8_t)(tag + 2u));
	seedbuf(z, sizeof(z), (uint8_t)(tag + 3u));
	TT_EQ_INT(osk_pqc_keypair(p->ek, p->dk, p->x_pub, d, z), OSK_OK, "Schluesselpaar");
}

static void roundtrip(void)
{
	struct party bob;
	uint8_t x_ct[OSK_PQC_X_BYTES], x_ss[OSK_PQC_X_BYTES], m[32];
	uint8_t ct[OSK_PQC_CT_BYTES];
	uint8_t ss_a[OSK_PQC_SS_BYTES], ss_b[OSK_PQC_SS_BYTES];

	make_party(&bob, 10u);
	seedbuf(x_ct, sizeof(x_ct), 20u);
	seedbuf(x_ss, sizeof(x_ss), 21u);
	seedbuf(m, sizeof(m), 22u);

	TT_EQ_INT(osk_pqc_encaps(ct, ss_a, bob.ek, x_ct, x_ss, m), OSK_OK, "Kapseln");
	TT_EQ_INT(osk_pqc_decaps(ss_b, bob.dk, ct, x_ss), OSK_OK, "Entkapseln");
	TT_EQ_MEM(ss_b, ss_a, OSK_PQC_SS_BYTES, "beide Seiten haben dasselbe");

	/* Der fluechtige Punkt der Gegenseite steht im Schluesseltext -
	 * ohne ihn kann der Empfaenger sein ECDH nicht rechnen. */
	TT_EQ_MEM(ct, x_ct, OSK_PQC_X_BYTES, "x-Punkt steht vorn im Schluesseltext");
	TT_EQ_INT(OSK_PQC_CT_BYTES, 1120, "Laenge des Schluesseltexts");
	TT_EQ_INT(OSK_PQC_EK_BYTES, 1216, "Laenge des oeffentlichen Schluessels");
	TT_EQ_INT(OSK_PQC_DK_BYTES, 2432, "Laenge des geheimen Schluessels");
}

/* Der Kern der Regel: ohne den Anteil aus dem Chip entsteht kein
 * Geheimnis. Lauter Nullen ist bei X25519 kein Zufall, sondern das
 * Ergebnis eines Punktes kleiner Ordnung - also ein Angriff, der genau
 * darauf zielt, den Chip-Anteil wegzuraeumen. */
static void no_secret_without_x25519(void)
{
	struct party bob;
	uint8_t x_ct[OSK_PQC_X_BYTES], m[32];
	uint8_t zero[OSK_PQC_X_BYTES];
	uint8_t ct[OSK_PQC_CT_BYTES];
	uint8_t ss[OSK_PQC_SS_BYTES];

	make_party(&bob, 30u);
	seedbuf(x_ct, sizeof(x_ct), 31u);
	seedbuf(m, sizeof(m), 32u);
	for (size_t i = 0; i < sizeof(zero); i++) {
		zero[i] = 0u;
	}

	TT_EQ_INT(osk_pqc_encaps(ct, ss, bob.ek, x_ct, zero, m), OSK_ERR_ARG,
		  "Kapseln ohne X25519-Anteil wird abgewiesen");
	TT_EQ_INT(osk_pqc_decaps(ss, bob.dk, ct, zero), OSK_ERR_ARG,
		  "Entkapseln ohne X25519-Anteil wird abgewiesen");
	/* Und es gibt auch keinen zweiten Weg: alle drei Funktionen
	 * verlangen den Anteil. */
	TT_EQ_INT(osk_pqc_keypair(bob.ek, bob.dk, zero, m, m), OSK_ERR_ARG,
		  "Schluesselpaar ohne X25519-Punkt wird abgewiesen");
}

/* Jedes Bit beider Anteile muss in das Ergebnis eingehen. Faellt ein
 * Anteil heraus, bleibt das Ergebnis gleich - und genau das waere der
 * Weg, auf dem ML-KEM allein traegt (oder X25519 allein). */
static void both_parts_reach_the_result(void)
{
	struct party bob;
	uint8_t x_ct[OSK_PQC_X_BYTES], x_ss[OSK_PQC_X_BYTES], m[32];
	uint8_t ct[OSK_PQC_CT_BYTES];
	uint8_t base[OSK_PQC_SS_BYTES], other[OSK_PQC_SS_BYTES];
	int same_x = 0, same_m = 0;

	make_party(&bob, 40u);
	seedbuf(x_ct, sizeof(x_ct), 41u);
	seedbuf(x_ss, sizeof(x_ss), 42u);
	seedbuf(m, sizeof(m), 43u);
	osk_pqc_encaps(ct, base, bob.ek, x_ct, x_ss, m);

	/* Jedes der 32 Byte des X25519-Anteils einmal verbiegen. */
	for (size_t i = 0; i < OSK_PQC_X_BYTES; i++) {
		x_ss[i] ^= 0x80u;
		osk_pqc_encaps(ct, other, bob.ek, x_ct, x_ss, m);
		if (memcmp(other, base, OSK_PQC_SS_BYTES) == 0) {
			same_x++;
		}
		x_ss[i] ^= 0x80u;
	}
	TT_EQ_INT(same_x, 0, "jedes Byte des X25519-Anteils geht ein");

	/* Und jedes zweiunddreissigste Byte des ML-KEM-Zufalls. */
	for (size_t i = 0; i < 32u; i++) {
		m[i] ^= 0x01u;
		osk_pqc_encaps(ct, other, bob.ek, x_ct, x_ss, m);
		if (memcmp(other, base, OSK_PQC_SS_BYTES) == 0) {
			same_m++;
		}
		m[i] ^= 0x01u;
	}
	TT_EQ_INT(same_m, 0, "jedes Byte des ML-KEM-Anteils geht ein");
}

/* Das ganze Gespraech ist gebunden. Wer unterwegs einen der beiden
 * Schluesseltexte austauscht, bringt die zwei Seiten auf verschiedene
 * Ergebnisse - er kann kein gueltiges Geheimnis in einen anderen
 * Zusammenhang tragen. */
static void transcript_is_bound(void)
{
	struct party bob;
	uint8_t x_ct[OSK_PQC_X_BYTES], x_ss[OSK_PQC_X_BYTES], m[32];
	uint8_t ct[OSK_PQC_CT_BYTES];
	uint8_t ss_a[OSK_PQC_SS_BYTES], ss_b[OSK_PQC_SS_BYTES];

	make_party(&bob, 50u);
	seedbuf(x_ct, sizeof(x_ct), 51u);
	seedbuf(x_ss, sizeof(x_ss), 52u);
	seedbuf(m, sizeof(m), 53u);
	osk_pqc_encaps(ct, ss_a, bob.ek, x_ct, x_ss, m);

	/* Den X25519-Punkt im Schluesseltext austauschen, das ECDH-
	 * Ergebnis aber gleich lassen (so sieht es fuer den Empfaenger
	 * aus, wenn der Angreifer nur den Punkt dreht). */
	ct[0] ^= 0x01u;
	TT_EQ_INT(osk_pqc_decaps(ss_b, bob.dk, ct, x_ss), OSK_OK, "kein Fehler");
	TT_TRUE(memcmp(ss_a, ss_b, OSK_PQC_SS_BYTES) != 0,
		"gedrehter X25519-Punkt fuehrt zu einem anderen Geheimnis");
	ct[0] ^= 0x01u;

	/* Den ML-KEM-Schluesseltext verbiegen: innen verwirft ML-KEM
	 * still und liefert ein Ersatzgeheimnis. Nach aussen darf das
	 * kein Fehler sein, aber ein anderes Ergebnis. */
	ct[OSK_PQC_X_BYTES + 5u] ^= 0x01u;
	TT_EQ_INT(osk_pqc_decaps(ss_b, bob.dk, ct, x_ss), OSK_OK,
		  "verfaelschter ML-KEM-Text ist kein Fehler");
	TT_TRUE(memcmp(ss_a, ss_b, OSK_PQC_SS_BYTES) != 0,
		"verfaelschter ML-KEM-Text fuehrt zu einem anderen Geheimnis");
	ct[OSK_PQC_X_BYTES + 5u] ^= 0x01u;
}

/* Zwei verschiedene Gegenseiten mit demselben X25519-Anteil muessen
 * verschiedene Geheimnisse ergeben: der oeffentliche Schluessel geht
 * mit in die Mischung. */
static void public_key_is_bound(void)
{
	struct party a, b;
	uint8_t x_ct[OSK_PQC_X_BYTES], x_ss[OSK_PQC_X_BYTES], m[32];
	uint8_t ct[OSK_PQC_CT_BYTES];
	uint8_t ss_a[OSK_PQC_SS_BYTES], ss_b[OSK_PQC_SS_BYTES];

	make_party(&a, 60u);
	make_party(&b, 70u);
	seedbuf(x_ct, sizeof(x_ct), 61u);
	seedbuf(x_ss, sizeof(x_ss), 62u);
	seedbuf(m, sizeof(m), 63u);

	osk_pqc_encaps(ct, ss_a, a.ek, x_ct, x_ss, m);
	osk_pqc_encaps(ct, ss_b, b.ek, x_ct, x_ss, m);
	TT_TRUE(memcmp(ss_a, ss_b, OSK_PQC_SS_BYTES) != 0, "andere Gegenseite, anderes Geheimnis");
}

void test_hybrid(void)
{
	TT_CASE("Rundlauf und Aufbau der Puffer");
	roundtrip();

	TT_CASE("ohne den Anteil aus dem Chip entsteht kein Geheimnis");
	no_secret_without_x25519();

	TT_CASE("beide Anteile gehen in das Ergebnis ein");
	both_parts_reach_the_result();

	TT_CASE("das ganze Gespraech ist gebunden");
	transcript_is_bound();

	TT_CASE("der oeffentliche Schluessel ist gebunden");
	public_key_is_bound();
}
