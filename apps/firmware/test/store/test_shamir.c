/* Aufteilung in Umschlaege: Rechnen, Text, zu wenige Teile. */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "fake_crypto.h"
#include "osk_shamir.h"

static void fill_secret(uint8_t s[OSK_SHARE_SECRET])
{
	for (size_t i = 0; i < OSK_SHARE_SECRET; i++) {
		s[i] = (uint8_t)(0x11u * (i + 1u) + i);
	}
}

static void case_split_combine(void)
{
	uint8_t secret[OSK_SHARE_SECRET];
	uint8_t back[OSK_SHARE_SECRET];
	osk_share_t shares[OSK_SHARE_MAX_N];

	t_init();
	fill_secret(secret);

	osk_test_case("Jede Auswahl von k Teilen ergibt dasselbe Geheimnis");
	T_OK(osk_shamir_split(secret, 3u, 5u, shares));
	for (int a = 0; a < 5; a++) {
		for (int b = a + 1; b < 5; b++) {
			for (int c = b + 1; c < 5; c++) {
				osk_share_t pick[3] = { shares[a], shares[b],
							shares[c] };

				T_OK(osk_shamir_combine(pick, 3u, back));
				T_MEM(back, secret, OSK_SHARE_SECRET);
			}
		}
	}

	osk_test_case("Mehr Teile als noetig stoeren nicht");
	T_OK(osk_shamir_combine(shares, 5u, back));
	T_MEM(back, secret, OSK_SHARE_SECRET);

	osk_test_case("Zwei von drei genuegen nicht - und ergeben nichts");
	memset(back, 0xAB, sizeof(back));
	T_RES(osk_shamir_combine(shares, 2u, back), OSK_ERR_DENIED);

	osk_test_case("Derselbe Umschlag zweimal ist kein zweiter Teil");
	osk_share_t twice[3] = { shares[0], shares[0], shares[1] };

	T_RES(osk_shamir_combine(twice, 3u, back), OSK_ERR_ARG);

	osk_test_case("Ein falscher Teil wird erkannt, nicht geraten");
	osk_share_t broken[3] = { shares[0], shares[1], shares[2] };

	broken[1].y[7] ^= 0x40u;
	T_RES(osk_shamir_combine(broken, 3u, back), OSK_ERR_DENIED);

	osk_test_case("Zwei von zwei");
	T_OK(osk_shamir_split(secret, 2u, 2u, shares));
	T_OK(osk_shamir_combine(shares, 2u, back));
	T_MEM(back, secret, OSK_SHARE_SECRET);

	osk_test_case("Sechzehn von sechzehn");
	T_OK(osk_shamir_split(secret, 16u, 16u, shares));
	T_OK(osk_shamir_combine(shares, 16u, back));
	T_MEM(back, secret, OSK_SHARE_SECRET);

	osk_test_case("Unsinnige Vorgaben werden abgelehnt");
	T_RES(osk_shamir_split(secret, 1u, 3u, shares), OSK_ERR_ARG);
	T_RES(osk_shamir_split(secret, 4u, 3u, shares), OSK_ERR_ARG);
	T_RES(osk_shamir_split(secret, 2u, OSK_SHARE_MAX_N + 1u, shares),
	      OSK_ERR_ARG);
}

static void case_text(void)
{
	uint8_t secret[OSK_SHARE_SECRET];
	uint8_t back[OSK_SHARE_SECRET];
	osk_share_t shares[3];
	char text[OSK_SHARE_TEXT_MAX];

	t_init();
	fill_secret(secret);
	T_OK(osk_shamir_split(secret, 2u, 3u, shares));

	osk_test_case("Ein Umschlag laesst sich ausdrucken und abschreiben");
	T_OK(osk_shamir_share_text(&shares[0], text, sizeof(text)));
	T_TRUE(strlen(text) > 70u);
	T_TRUE(strncmp(text, "OSK1-", 5) == 0);
	for (const char *p = text; *p != '\0'; p++) {
		bool ok = (*p >= '0' && *p <= '9') ||
			  (*p >= 'A' && *p <= 'F') || *p == '-' || *p == 'O' ||
			  *p == 'S' || *p == 'K';

		T_TRUE(ok);
	}

	osk_share_t again;

	T_OK(osk_shamir_share_parse(text, &again));
	T_EQ(again.k, shares[0].k);
	T_EQ(again.n, shares[0].n);
	T_EQ(again.index, shares[0].index);
	T_MEM(again.y, shares[0].y, OSK_SHARE_SECRET);

	osk_test_case("Klein geschrieben und mit Leerzeichen geht auch");
	char sloppy[OSK_SHARE_TEXT_MAX + 8];
	size_t pos = 0;

	for (const char *p = text; *p != '\0'; p++) {
		char c = *p;

		if (c >= 'A' && c <= 'F') {
			c = (char)(c - 'A' + 'a');
		}
		sloppy[pos++] = c;
		if (pos == 20u) {
			sloppy[pos++] = ' ';
		}
	}
	sloppy[pos] = '\0';
	T_OK(osk_shamir_share_parse(sloppy, &again));
	T_MEM(again.y, shares[0].y, OSK_SHARE_SECRET);

	osk_test_case("Aus Text zusammengesetzt kommt dasselbe heraus");
	osk_share_t pair[2];
	char t2[OSK_SHARE_TEXT_MAX];

	T_OK(osk_shamir_share_text(&shares[1], t2, sizeof(t2)));
	T_OK(osk_shamir_share_parse(text, &pair[0]));
	T_OK(osk_shamir_share_parse(t2, &pair[1]));
	T_OK(osk_shamir_combine(pair, 2u, back));
	T_MEM(back, secret, OSK_SHARE_SECRET);

	osk_test_case("Ein Tippfehler im Umschlag wird gemeldet");
	char bad[OSK_SHARE_TEXT_MAX];

	memcpy(bad, text, sizeof(bad));
	bad[12] = (bad[12] == 'A') ? 'B' : 'A';
	T_OK(osk_shamir_share_parse(bad, &pair[0]));
	T_RES(osk_shamir_combine(pair, 2u, back), OSK_ERR_DENIED);

	osk_test_case("Kein Umschlag, nur Text: abgelehnt");
	T_RES(osk_shamir_share_parse("hallo welt", &again), OSK_ERR_FORMAT);
	T_RES(osk_shamir_share_parse("OSK1-2-3-01-XYZ", &again),
	      OSK_ERR_FORMAT);
}

void test_shamir(void)
{
	case_split_combine();
	case_text();
}
