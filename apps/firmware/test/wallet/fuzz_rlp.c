/* Fuzzlauf ueber den RLP-Leser und die Ethereum-Transaktionen.
 *
 * Aufruf: fuzz_rlp [Sekunden] [Startwert]
 *
 * Geprueft werden vier Zusicherungen:
 *  1. Kein Aufruf bringt den Prozess um oder schreibt ueber einen Puffer.
 *  2. Der Leser kommt nie ueber das Ende: die Summe aller gelesenen
 *     Stuecke liegt immer im Puffer.
 *  3. Eine gelesene Transaktion laesst sich anzeigen - osk_eth_view faellt
 *     nie, und eine als lesbar gemeldete Ansicht hat Empfaenger, Betrag
 *     und Gebuehr.
 *  4. Was die Schreibseite erzeugt, liest die Leseseite wieder gleich.
 * Und: ein Lauf ohne eine einzige gelesene Transaktion meldet das. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wallet_rlp.h"
#include "wallet_eth.h"
#include "mocks.h"

static uint64_t g_rng;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

static size_t rnd_below(size_t n)
{
	return (n == 0) ? 0 : (size_t)(rnd() % n);
}

static size_t unhex(const char *in, uint8_t *out, size_t cap)
{
	size_t n = 0;

	for (const char *s = in; *s != '\0'; s += 2) {
		unsigned value = 0;

		if (s[1] == '\0')
			return 0;
		for (unsigned k = 0; k < 2; k++) {
			char c = s[k];

			value <<= 4;
			if (c >= '0' && c <= '9')
				value |= (unsigned)(c - '0');
			else if (c >= 'a' && c <= 'f')
				value |= (unsigned)(c - 'a') + 10u;
			else if (c >= 'A' && c <= 'F')
				value |= (unsigned)(c - 'A') + 10u;
			else
				return 0;
		}
		if (n >= cap)
			return 0;
		out[n++] = (uint8_t)value;
	}
	return n;
}

static size_t mutate(const uint8_t *in, size_t len, uint8_t *out, size_t cap)
{
	size_t n = len;
	unsigned rounds;

	if (n > cap)
		n = cap;
	memcpy(out, in, n);
	rounds = 1u + (rnd() % 5u);
	for (unsigned i = 0; i < rounds; i++) {
		switch (rnd() % 6u) {
		case 0:
			if (n > 0)
				out[rnd_below(n)] = (uint8_t)rnd();
			break;
		case 1:
			if (n > 0)
				out[rnd_below(n)] ^= (uint8_t)(1u << (rnd() % 8u));
			break;
		case 2:
			if (n > 1u)
				n = 1u + rnd_below(n - 1u);
			break;
		case 3:
			if (n < cap)
				out[n++] = (uint8_t)rnd();
			break;
		case 4:
			/* Eine lange Laengenangabe: der Weg, auf dem ein
			 * Leser ueber das Ende hinauslaeuft. */
			if (n > 0)
				out[rnd_below(n)] = (uint8_t)(0xB8u + (rnd() % 8u));
			break;
		default:
			if (n > 0)
				out[rnd_below(n)] = (uint8_t)(0xF8u + (rnd() % 8u));
			break;
		}
	}
	return n;
}

/* Jedes Stueck muss im Puffer liegen - sonst hat der Leser hinausgelesen. */
static bool walk(const uint8_t *p, size_t len, unsigned depth)
{
	osk_rlp_reader_t r;
	osk_rlp_item_t item;

	if (depth > 4u)
		return true;
	osk_rlp_reader_init(&r, p, len);
	for (;;) {
		osk_result_t res = osk_rlp_next(&r, &item);

		if (res == OSK_ERR_NOT_FOUND)
			return true;
		if (res != OSK_OK)
			return true; /* abgelehnt ist in Ordnung */
		if (item.data < p || item.data + item.len > p + len)
			return false;
		if (item.kind == OSK_RLP_LIST && !walk(item.data, item.len, depth + 1u))
			return false;
		{
			uint64_t small = 0;
			uint8_t big[32];

			(void)osk_rlp_as_u64(&item, &small);
			(void)osk_rlp_as_u256(&item, big);
		}
	}
}

static const char *const TEMPLATES[] = {
    /* alte Art mit Kettennummer (EIP-155) */
    "ec098504a817c800825208943535353535353535353535353535353535353535880de0b6b3a764000080"
    "018080",
    /* EIP-1559 */
    "02f00109843b9aca008506fc23ac00825208943535353535353535353535353535353535353535880de0"
    "b6b3a764000080c0",
    /* EIP-2930 */
    "01eb01098504a817c800825208943535353535353535353535353535353535353535880de0b6b3a76400"
    "0080c0",
    /* mit Daten, also ein Vertragsaufruf */
    "f0098504a817c800825208943535353535353535353535353535353535353535880de0b6b3a764000084"
    "a9059cbb018080",
};

#define TEMPLATE_COUNT (sizeof(TEMPLATES) / sizeof(TEMPLATES[0]))

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;
	static uint8_t templ[TEMPLATE_COUNT][256];
	static size_t templ_len[TEMPLATE_COUNT];
	static uint8_t msg[512];
	unsigned long runs = 0, parsed = 0, readable = 0, pure_random = 0;
	time_t start;

	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0x1f1f2f2f3f3fULL;
	if (g_rng == 0)
		g_rng = 1;

	for (size_t i = 0; i < TEMPLATE_COUNT; i++) {
		templ_len[i] = unhex(TEMPLATES[i], templ[i], sizeof(templ[i]));
		if (templ_len[i] == 0) {
			printf("FEHLER: Vorlage %zu ist leer\n", i);
			return 1;
		}
	}
	/* Die Vorlagen muessen selbst lesbar sein, sonst prueft der Lauf
	 * nur, dass alles abgelehnt wird. */
	for (size_t i = 0; i < TEMPLATE_COUNT; i++) {
		osk_eth_tx_t tx;

		if (osk_eth_parse_tx(templ[i], templ_len[i], &tx) != OSK_OK) {
			printf("FEHLER: Vorlage %zu wird selbst abgelehnt\n", i);
			return 1;
		}
	}

	start = time(NULL);
	while (time(NULL) - start < secs) {
		osk_eth_tx_t tx;
		size_t len;

		if ((rnd() % 6u) == 0u) {
			len = 1u + rnd_below(120);
			for (size_t i = 0; i < len; i++)
				msg[i] = (uint8_t)rnd();
			pure_random++;
		} else {
			size_t which = rnd_below(TEMPLATE_COUNT);

			len = mutate(templ[which], templ_len[which], msg, sizeof(msg));
		}
		runs++;

		if (!walk(msg, len, 0)) {
			printf("FEHLER: der Leser hat ueber das Ende hinausgelesen "
			       "(%zu Bytes)\n",
			       len);
			return 1;
		}

		if (osk_eth_parse_tx(msg, len, &tx) != OSK_OK)
			continue;
		parsed++;
		{
			osk_wallet_view_t view;
			uint8_t hash[32];

			if (osk_eth_view(&tx, "ETH", &view) != OSK_OK) {
				printf("FEHLER: osk_eth_view faellt bei %zu Bytes\n", len);
				return 1;
			}
			if (view.readable) {
				readable++;
				if (view.recipient[0] == '\0' || view.amount[0] == '\0' ||
				    view.fee[0] == '\0') {
					printf("FEHLER: als lesbar gemeldet, aber ein Feld "
					       "ist leer\n");
					return 1;
				}
			}
			if (osk_eth_tx_hash(msg, len, hash) != OSK_OK) {
				printf("FEHLER: Hash scheitert an gelesenen Bytes\n");
				return 1;
			}
		}
	}

	/* Schreiben und wieder lesen muss denselben Wert ergeben. */
	for (unsigned i = 0; i < 2000; i++) {
		uint8_t buf[16];
		uint64_t value = ((uint64_t)rnd() << 32) | rnd();
		size_t written = osk_rlp_write_u64(value, buf, sizeof(buf));
		osk_rlp_reader_t r;
		osk_rlp_item_t item;
		uint64_t back = 0;

		if (written == 0) {
			printf("FEHLER: Schreiben scheitert\n");
			return 1;
		}
		osk_rlp_reader_init(&r, buf, written);
		if (osk_rlp_next(&r, &item) != OSK_OK || !osk_rlp_at_end(&r)) {
			printf("FEHLER: Geschriebenes ist nicht lesbar\n");
			return 1;
		}
		if (value == 0) {
			if (item.len != 0) {
				printf("FEHLER: die Null ist nicht die leere Folge\n");
				return 1;
			}
			continue;
		}
		if (osk_rlp_as_u64(&item, &back) != OSK_OK || back != value) {
			printf("FEHLER: hin und zurueck ergibt etwas anderes\n");
			return 1;
		}
	}

	printf("fuzz_rlp: %lu Durchgaenge, %lu reiner Zufall, %lu gelesen, %lu lesbar\n", runs,
	       pure_random, parsed, readable);
	if (runs == 0) {
		printf("FEHLER: kein einziger Durchgang\n");
		return 1;
	}
	if (parsed == 0) {
		printf("FEHLER: keine einzige Transaktion kam durch den Leser\n");
		return 1;
	}
	if (readable == 0) {
		printf("FEHLER: keine einzige Transaktion war anzeigbar - der Lauf hat die "
		       "Anzeige nie erreicht\n");
		return 1;
	}
	printf("fuzzlauf: ok\n");
	return 0;
}
