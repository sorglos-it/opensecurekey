/* ML-KEM-768 gegen die amtlichen Testwerte des NIST.
 *
 * Das ist der Teil des Pruefsatzes, der etwas beweist. Alles andere
 * hier - Rundlauf, Verfaelschung, Fehlerwege - prueft diese Umsetzung
 * gegen sich selbst, und das ist bei einem selbstgebauten Verfahren
 * wenig wert: ein Rundlauf geht auch auf, wenn beide Seiten denselben
 * Fehler machen. Die Werte in kat_mlkem768.h kommen von ausserhalb.
 *
 * Siehe docs/pqc.md fuer die Herkunft und fuer das, was dieser Beleg
 * deckt und was nicht.
 */
#define OSK_PQC_INTERNAL 1
#include "tiny_test.h"
#include "pqc_mlkem.h"
#include "pqc_keccak.h"
#include "kat_mlkem768.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static void keygen_kat(void)
{
	for (size_t i = 0; i < COUNT(k_kat_keygen); i++) {
		uint8_t d[32], z[32], want[32], got[32];
		uint8_t ek[OSK_MLKEM_EK_BYTES];
		uint8_t dk[OSK_MLKEM_DK_BYTES];

		TT_EQ_INT(tt_unhex(d, sizeof(d), k_kat_keygen[i].d), 32, "Keim d");
		TT_EQ_INT(tt_unhex(z, sizeof(z), k_kat_keygen[i].z), 32, "Keim z");

		osk_mlkem_keygen(ek, dk, d, z);

		osk_sha3_256(got, ek, sizeof(ek));
		tt_unhex(want, sizeof(want), k_kat_keygen[i].ek_sha3);
		TT_EQ_MEM(got, want, 32u, "oeffentlicher Schluessel (SHA3-256)");

		osk_sha3_256(got, dk, sizeof(dk));
		tt_unhex(want, sizeof(want), k_kat_keygen[i].dk_sha3);
		TT_EQ_MEM(got, want, 32u, "geheimer Schluessel (SHA3-256)");
	}
}

static void encaps_kat(void)
{
	for (size_t i = 0; i < COUNT(k_kat_encaps); i++) {
		uint8_t ek[OSK_MLKEM_EK_BYTES];
		uint8_t m[32], want[32], got[32];
		uint8_t ct[OSK_MLKEM_CT_BYTES];
		uint8_t ss[OSK_MLKEM_SS_BYTES];

		TT_EQ_INT(tt_unhex(ek, sizeof(ek), k_kat_encaps[i].ek), OSK_MLKEM_EK_BYTES,
			  "Laenge des oeffentlichen Schluessels");
		TT_EQ_INT(tt_unhex(m, sizeof(m), k_kat_encaps[i].m), 32, "Zufall m");

		TT_EQ_INT(osk_mlkem_encaps(ct, ss, ek, m), OSK_OK, "Kapseln geht");

		osk_sha3_256(got, ct, sizeof(ct));
		tt_unhex(want, sizeof(want), k_kat_encaps[i].c_sha3);
		TT_EQ_MEM(got, want, 32u, "Schluesseltext (SHA3-256)");

		tt_unhex(want, sizeof(want), k_kat_encaps[i].k);
		TT_EQ_MEM(ss, want, OSK_MLKEM_SS_BYTES, "gemeinsames Geheimnis");
	}
}

/* Die amtlichen Entkapsel-Faelle haben zwei Sorten: gueltige
 * Schluesseltexte und verfaelschte. Bei den verfaelschten ist der
 * amtliche Sollwert NICHT ein Fehler, sondern das Ersatzgeheimnis aus z
 * (stille Verwerfung). Dass dieser Wert stimmt, ist der scharfe Teil -
 * er haengt an z, am ganzen Schluesseltext und an der Reihenfolge, in
 * der beides in SHAKE256 geht. */
static void decaps_kat(void)
{
	for (size_t i = 0; i < COUNT(k_kat_decaps); i++) {
		uint8_t dk[OSK_MLKEM_DK_BYTES];
		uint8_t ct[OSK_MLKEM_CT_BYTES];
		uint8_t ss[OSK_MLKEM_SS_BYTES];
		uint8_t want[32];

		printf("    Fall %u: %s\n", (unsigned)i + 1u, k_kat_decaps[i].reason);
		TT_EQ_INT(tt_unhex(dk, sizeof(dk), k_kat_decaps[i].dk), OSK_MLKEM_DK_BYTES,
			  "Laenge des geheimen Schluessels");
		TT_EQ_INT(tt_unhex(ct, sizeof(ct), k_kat_decaps[i].c), OSK_MLKEM_CT_BYTES,
			  "Laenge des Schluesseltexts");

		TT_EQ_INT(osk_mlkem_decaps(ss, dk, ct), OSK_OK,
			  "Entkapseln meldet keinen Fehler, auch nicht bei verfaelschtem Text");
		tt_unhex(want, sizeof(want), k_kat_decaps[i].k);
		TT_EQ_MEM(ss, want, OSK_MLKEM_SS_BYTES, "gemeinsames Geheimnis");
	}
}

/* Rundlauf mit eigenen Schluesseln. Beweist nichts ueber die
 * Richtigkeit gegenueber dem Standard - steht aber hier, weil es die
 * drei Teile als Ganzes durchlaeuft und ein Fehler in einem Weg, der in
 * keinem amtlichen Fall vorkommt, sonst unbemerkt bliebe. */
static void roundtrip(void)
{
	for (size_t i = 0; i < COUNT(k_kat_keygen); i++) {
		uint8_t d[32], z[32], m[32];
		uint8_t ek[OSK_MLKEM_EK_BYTES];
		uint8_t dk[OSK_MLKEM_DK_BYTES];
		uint8_t ct[OSK_MLKEM_CT_BYTES];
		uint8_t ss_a[OSK_MLKEM_SS_BYTES], ss_b[OSK_MLKEM_SS_BYTES];

		tt_unhex(d, sizeof(d), k_kat_keygen[i].d);
		tt_unhex(z, sizeof(z), k_kat_keygen[i].z);
		osk_sha3_256(m, d, sizeof(d));

		osk_mlkem_keygen(ek, dk, d, z);
		TT_EQ_INT(osk_mlkem_encaps(ct, ss_a, ek, m), OSK_OK, "Kapseln");
		TT_EQ_INT(osk_mlkem_decaps(ss_b, dk, ct), OSK_OK, "Entkapseln");
		TT_EQ_MEM(ss_b, ss_a, OSK_MLKEM_SS_BYTES, "beide Seiten haben dasselbe");
	}
}

/* Jedes einzelne Byte des Schluesseltexts muss zaehlen. Faellt eines
 * heraus - weil ein Stueck des Puffers nie angesehen wird -, kann ein
 * Angreifer dort schreiben, was er will. */
static void every_byte_counts(void)
{
	uint8_t d[32], z[32], m[32];
	uint8_t ek[OSK_MLKEM_EK_BYTES];
	uint8_t dk[OSK_MLKEM_DK_BYTES];
	uint8_t ct[OSK_MLKEM_CT_BYTES];
	uint8_t ss_good[OSK_MLKEM_SS_BYTES], ss_bad[OSK_MLKEM_SS_BYTES];
	int ignored = 0;

	tt_unhex(d, sizeof(d), k_kat_keygen[0].d);
	tt_unhex(z, sizeof(z), k_kat_keygen[0].z);
	osk_sha3_256(m, d, sizeof(d));
	osk_mlkem_keygen(ek, dk, d, z);
	osk_mlkem_encaps(ct, ss_good, ek, m);
	osk_mlkem_decaps(ss_good, dk, ct);

	/* Nicht alle 1088 Byte einzeln - das waere bei dieser Umsetzung
	 * rund eine halbe Minute. Jedes 37. Byte trifft jeden Abschnitt
	 * des Schluesseltexts. */
	for (size_t i = 0; i < OSK_MLKEM_CT_BYTES; i += 37u) {
		ct[i] ^= 0x40u;
		TT_EQ_INT(osk_mlkem_decaps(ss_bad, dk, ct), OSK_OK,
			  "verfaelschter Text ist kein Fehler");
		if (memcmp(ss_bad, ss_good, OSK_MLKEM_SS_BYTES) == 0) {
			ignored++;
		}
		ct[i] ^= 0x40u;
	}
	TT_EQ_INT(ignored, 0, "kein Byte des Schluesseltexts wird uebersehen");
}

/* Ein oeffentlicher Schluessel, dessen Werte nicht reduziert sind, muss
 * abgewiesen werden (FIPS 203, 7.2). Sonst gibt es zwei Schreibweisen
 * fuer denselben Schluessel - und damit zwei verschiedene Geheimnisse
 * fuer dieselbe Gegenseite. */
static void bad_public_key_is_refused(void)
{
	uint8_t ek[OSK_MLKEM_EK_BYTES];
	uint8_t ct[OSK_MLKEM_CT_BYTES];
	uint8_t ss[OSK_MLKEM_SS_BYTES];
	uint8_t m[32];

	tt_unhex(ek, sizeof(ek), k_kat_encaps[0].ek);
	tt_unhex(m, sizeof(m), k_kat_encaps[0].m);
	TT_EQ_INT(osk_mlkem_encaps(ct, ss, ek, m), OSK_OK, "unveraendert geht");

	/* Die ersten zwei Byte tragen den ersten Koeffizienten (12 Bit,
	 * kleinstes Bit zuerst). 0xff 0x0f ist 4095 und damit groesser
	 * als q. */
	ek[0] = 0xffu;
	ek[1] = (uint8_t)((ek[1] & 0xf0u) | 0x0fu);
	TT_EQ_INT(osk_mlkem_encaps(ct, ss, ek, m), OSK_ERR_FORMAT,
		  "nicht reduzierter Schluessel wird abgewiesen");
}

/* Ein geheimer Schluessel, in dem der mitgefuehrte oeffentliche Teil
 * nicht zu seinem Hash passt, ist kaputter Flash - kein Angriff von der
 * Leitung. Das darf gemeldet werden, und es muss gemeldet werden, weil
 * sonst still mit dem falschen Schluessel gerechnet wird. */
static void broken_secret_key_is_reported(void)
{
	uint8_t d[32], z[32];
	uint8_t ek[OSK_MLKEM_EK_BYTES];
	uint8_t dk[OSK_MLKEM_DK_BYTES];
	uint8_t ct[OSK_MLKEM_CT_BYTES];
	uint8_t ss[OSK_MLKEM_SS_BYTES];
	uint8_t m[32];

	tt_unhex(d, sizeof(d), k_kat_keygen[1].d);
	tt_unhex(z, sizeof(z), k_kat_keygen[1].z);
	osk_sha3_256(m, z, sizeof(z));
	osk_mlkem_keygen(ek, dk, d, z);
	osk_mlkem_encaps(ct, ss, ek, m);
	TT_EQ_INT(osk_mlkem_decaps(ss, dk, ct), OSK_OK, "unveraendert geht");

	dk[OSK_MLKEM_DK_BYTES - 33u] ^= 0x01u; /* letztes Byte des Hashs */
	TT_EQ_INT(osk_mlkem_decaps(ss, dk, ct), OSK_ERR_FORMAT, "kaputter dk wird gemeldet");
}

void test_mlkem(void)
{
	TT_CASE("Schluesselerzeugung, 25 amtliche Faelle");
	keygen_kat();

	TT_CASE("Kapseln, 3 amtliche Faelle");
	encaps_kat();

	TT_CASE("Entkapseln, 4 amtliche Faelle (2 gueltig, 2 verfaelscht)");
	decaps_kat();

	TT_CASE("Rundlauf mit eigenen Schluesseln, 25 Paare");
	roundtrip();

	TT_CASE("jedes Byte des Schluesseltexts zaehlt");
	every_byte_counts();

	TT_CASE("nicht reduzierter oeffentlicher Schluessel");
	bad_public_key_is_refused();

	TT_CASE("geheimer Schluessel passt nicht zu seinem Hash");
	broken_secret_key_is_reported();
}
