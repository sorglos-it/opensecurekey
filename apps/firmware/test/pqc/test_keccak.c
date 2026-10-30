/* Keccak gegen eine fremde Umsetzung (hashlib aus CPython).
 *
 * Warum das hier ganz vorn steht: ML-KEM ist zu einem guten Teil
 * Keccak. Ist SHAKE128 falsch, ist die oeffentliche Matrix A falsch, und
 * dann stimmt kein einziger amtlicher Wert - man sucht aber im
 * Zahlenwerk. Darum erst der Hash, dann das Verfahren.
 *
 * Die Laengen sind um die Blockgroessen herum gewaehlt (135, 136, 137
 * bei einer Rate von 136): genau dort sitzen die Fehler im Abschluss.
 */
#include "tiny_test.h"
#include "pqc_keccak.h"
#include "kat_mlkem768.h"

#define MAX_MSG 256

static void one_case(const struct kat_keccak *kat)
{
	uint8_t msg[MAX_MSG];
	uint8_t want[64];
	uint8_t got[64];
	size_t len = tt_unhex(msg, sizeof(msg), kat->msg);

	osk_sha3_256(got, msg, len);
	tt_unhex(want, sizeof(want), kat->sha3_256);
	TT_EQ_MEM(got, want, 32u, "SHA3-256");

	osk_sha3_512(got, msg, len);
	tt_unhex(want, sizeof(want), kat->sha3_512);
	TT_EQ_MEM(got, want, 64u, "SHA3-512");

	osk_shake256(got, 64u, msg, len);
	tt_unhex(want, sizeof(want), kat->shake256_64);
	TT_EQ_MEM(got, want, 64u, "SHAKE256, 64 Byte");

	{
		struct osk_keccak st;

		osk_keccak_init(&st, OSK_SHAKE128_RATE, OSK_KECCAK_PAD_SHAKE);
		osk_keccak_absorb(&st, msg, len);
		osk_keccak_squeeze(&st, got, 64u);
		osk_keccak_wipe(&st);
		tt_unhex(want, sizeof(want), kat->shake128_64);
		TT_EQ_MEM(got, want, 64u, "SHAKE128, 64 Byte");
	}
}

/* Stueckweise hineingeben und stueckweise herausziehen muss dasselbe
 * ergeben wie in einem Zug. ML-KEM nutzt beides: die Matrix wird in
 * Haeppchen von drei Byte gezogen, der Keim in zwei Stuecken gegeben. */
static void piecewise(void)
{
	uint8_t msg[MAX_MSG];
	uint8_t whole[64], parts[64];
	size_t len = tt_unhex(msg, sizeof(msg), k_kat_keccak[7].msg);
	struct osk_keccak st;

	osk_shake256(whole, sizeof(whole), msg, len);

	osk_keccak_init(&st, OSK_SHAKE256_RATE, OSK_KECCAK_PAD_SHAKE);
	for (size_t i = 0; i < len; i++) {
		osk_keccak_absorb(&st, msg + i, 1u);
	}
	for (size_t i = 0; i < sizeof(parts); i += 3u) {
		size_t take = sizeof(parts) - i < 3u ? sizeof(parts) - i : 3u;

		osk_keccak_squeeze(&st, parts + i, take);
	}
	osk_keccak_wipe(&st);
	TT_EQ_MEM(parts, whole, sizeof(whole), "stueckweise wie in einem Zug");
}

/* Nach dem ersten Ziehen darf nichts mehr hineingehen. Sonst haengt das
 * Ergebnis davon ab, in welcher Reihenfolge ein Aufrufer die Funktionen
 * ruft - und so ein Fehler faellt nirgends auf. */
static void absorb_after_squeeze_is_ignored(void)
{
	uint8_t a[32], b[32];
	struct osk_keccak st;
	const uint8_t extra[4] = {1u, 2u, 3u, 4u};

	osk_keccak_init(&st, OSK_SHAKE256_RATE, OSK_KECCAK_PAD_SHAKE);
	osk_keccak_absorb(&st, extra, sizeof(extra));
	osk_keccak_squeeze(&st, a, sizeof(a));
	osk_keccak_absorb(&st, extra, sizeof(extra));
	osk_keccak_squeeze(&st, b, sizeof(b));
	osk_keccak_wipe(&st);

	/* b ist die Fortsetzung des Stroms, nicht ein neuer Strom. */
	osk_keccak_init(&st, OSK_SHAKE256_RATE, OSK_KECCAK_PAD_SHAKE);
	osk_keccak_absorb(&st, extra, sizeof(extra));
	{
		uint8_t both[64];

		osk_keccak_squeeze(&st, both, sizeof(both));
		osk_keccak_wipe(&st);
		TT_EQ_MEM(a, both, 32u, "erste 32 Byte");
		TT_EQ_MEM(b, both + 32, 32u, "zweite 32 Byte, Strom laeuft weiter");
	}
}

static void wipe_clears(void)
{
	struct osk_keccak st;
	uint8_t out[32];
	uint8_t zero[sizeof(st)];
	const uint8_t *raw = (const uint8_t *)&st;
	int all_zero = 1;

	osk_keccak_init(&st, OSK_SHA3_256_RATE, OSK_KECCAK_PAD_SHA3);
	osk_keccak_absorb(&st, (const uint8_t *)"geheim", 6u);
	osk_keccak_squeeze(&st, out, sizeof(out));
	osk_keccak_wipe(&st);
	for (size_t i = 0; i < sizeof(zero); i++) {
		zero[i] = 0u;
		if (raw[i] != 0u) {
			all_zero = 0;
		}
	}
	TT_TRUE(all_zero, "Zustand nach dem Loeschen leer");
	TT_EQ_MEM(raw, zero, sizeof(zero), "Zustand nach dem Loeschen leer (Byte)");
}

void test_keccak(void)
{
	TT_CASE("SHA3-256, SHA3-512, SHAKE128, SHAKE256 gegen hashlib");
	for (size_t i = 0; i < sizeof(k_kat_keccak) / sizeof(k_kat_keccak[0]); i++) {
		one_case(&k_kat_keccak[i]);
	}

	TT_CASE("stueckweise hineingeben und herausziehen");
	piecewise();

	TT_CASE("nach dem Ziehen nichts mehr hineingeben");
	absorb_after_squeeze_is_ignored();

	TT_CASE("Zustand wird geloescht");
	wipe_clears();
}
