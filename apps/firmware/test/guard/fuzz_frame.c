/* Fuzzlauf ueber den Rahmen der Befehle 14 bis 18.
 *
 * Hier kommt ein Angreifer zuerst an: er schickt Bytes in den eigenen
 * Kanal, und der Parser muss sie abweisen, ohne ueber einen Puffer zu
 * laufen. Der Lauf wirft erst Zufall hinein, dann verbogene Fassungen
 * echter Nachrichten - reiner Zufall kommt an den Pflichtfeldern nicht weit
 * genug vorbei.
 *
 * Aufruf: fuzz_frame [Sekunden] [Startwert]
 * Mit -fsanitize=address,undefined uebersetzt (siehe Makefile) wird jeder
 * Ueberlauf zum Abbruch, nicht zu einem stillen Fehler.
 *
 * Zwei Zusicherungen:
 *   1. Kein Aufruf darf den Prozess umbringen oder ueber Puffer schreiben.
 *   2. Jede Antwort, die ueberhaupt kommt, ist lesbares CBOR und traegt
 *      `v`, `s` und `r` - der Schreiber darf nie ausgeben, was unser
 *      eigener strenger Leser ablehnt.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mocks.h"
#include "guard_helper.h"
#include "osk_guard_keys.h"

static uint64_t g_rng;

static uint32_t rnd(void)
{
	/* xorshift: klein, vorhersagbar, reicht fuer diesen Zweck */
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

static size_t rnd_below(size_t n)
{
	return (n == 0) ? 0 : (size_t)(rnd() % n);
}

/* Eine echte, vollstaendige sign-Nachricht als Vorlage. */
static size_t sample_sign(uint8_t *out, size_t cap)
{
	osk_cbor_writer_t w;
	uint8_t hash[OSK_GUARD_HASH_LEN];

	memset(hash, 0x5a, sizeof(hash));
	osk_cbor_writer_init(&w, out, cap);
	osk_cbor_put_map(&w, 4u);
	osk_cbor_put_text(&w, "v");
	osk_cbor_put_uint(&w, OSK_GUARD_PROTO_VERSION);
	osk_cbor_put_text(&w, "c");
	osk_cbor_put_uint(&w, OSK_GUARD_CMD_SIGN);
	osk_cbor_put_text(&w, "s");
	osk_cbor_put_uint(&w, 1u);
	osk_cbor_put_text(&w, "d");
	osk_cbor_put_map(&w, 2u);
	osk_cbor_put_text(&w, "k");
	osk_cbor_put_text(&w, "prod-eu-1");
	osk_cbor_put_text(&w, "h");
	osk_cbor_put_bytes(&w, hash, sizeof(hash));
	return osk_cbor_writer_ok(&w) ? w.len : 0u;
}

/* Eine echte attest-Nachricht als zweite Vorlage. */
static size_t sample_attest(uint8_t *out, size_t cap)
{
	osk_cbor_writer_t w;
	uint8_t nonce[OSK_GUARD_NONCE_LEN];
	uint8_t hash[OSK_GUARD_HASH_LEN];

	memset(nonce, 0x11, sizeof(nonce));
	memset(hash, 0x22, sizeof(hash));
	osk_cbor_writer_init(&w, out, cap);
	osk_cbor_put_map(&w, 4u);
	osk_cbor_put_text(&w, "v");
	osk_cbor_put_uint(&w, OSK_GUARD_PROTO_VERSION);
	osk_cbor_put_text(&w, "c");
	osk_cbor_put_uint(&w, OSK_GUARD_CMD_ATTEST);
	osk_cbor_put_text(&w, "s");
	osk_cbor_put_uint(&w, 2u);
	osk_cbor_put_text(&w, "d");
	osk_cbor_put_map(&w, 2u);
	osk_cbor_put_text(&w, "ch");
	osk_cbor_put_bytes(&w, nonce, sizeof(nonce));
	osk_cbor_put_text(&w, "bh");
	osk_cbor_put_bytes(&w, hash, sizeof(hash));
	return osk_cbor_writer_ok(&w) ? w.len : 0u;
}

/* Prueft Zusicherung 2 an einer Antwort. */
static int reply_is_sane(const gh_reply_t *reply)
{
	if (reply->len == 0u) {
		return 1; /* keine Antwort ist erlaubt (Puffer zu klein) */
	}
	return reply->ok ? 1 : 0;
}

int main(int argc, char **argv)
{
	const unsigned seconds = argc > 1 ? (unsigned)atoi(argv[1]) : 5u;
	g_rng = argc > 2 ? (uint64_t)strtoull(argv[2], NULL, 10) : 0x2b1c9f3du;
	if (g_rng == 0u) {
		g_rng = 1u;
	}

	uint8_t templates[2][GH_BUF];
	size_t template_len[2];
	gh_reply_t reply;
	uint8_t msg[GH_BUF];
	unsigned long rounds = 0;
	unsigned long answers = 0;
	int bad = 0;

	mock_reset_all();
	mock_se050_setup_guard();
	osk_guard_keys_reset();
	if (osk_guard_node_register("prod-eu-1", 9u, OSK_GUARD_CURVE_ED25519) != OSK_OK) {
		printf("ABBRUCH: Schluessel liess sich nicht eintragen\n");
		return 2;
	}
	template_len[0] = sample_sign(templates[0], sizeof(templates[0]));
	template_len[1] = sample_attest(templates[1], sizeof(templates[1]));
	if (template_len[0] == 0u || template_len[1] == 0u) {
		/* Eine Messung, deren Vorbereitung nicht gewirkt hat, nennt
		 * keine Zahl - sie bricht ab. */
		printf("ABBRUCH: Vorlagen liessen sich nicht bauen\n");
		return 2;
	}

	const time_t until = time(NULL) + (time_t)seconds;
	while (time(NULL) < until) {
		size_t len;
		if ((rnd() & 1u) == 0u) {
			/* reiner Zufall */
			len = 1u + rnd_below(96u);
			for (size_t i = 0; i < len; i++) {
				msg[i] = (uint8_t)rnd();
			}
		} else {
			/* eine Vorlage, an einer Stelle verbogen */
			const unsigned which = rnd() & 1u;
			len = template_len[which];
			memcpy(msg, templates[which], len);
			const unsigned hits = 1u + (rnd() % 3u);
			for (unsigned h = 0; h < hits; h++) {
				msg[rnd_below(len)] = (uint8_t)rnd();
			}
			if ((rnd() % 4u) == 0u) {
				len = 1u + rnd_below(len); /* abgeschnitten */
			}
		}

		const osk_link_t link = (rnd() & 1u) ? OSK_LINK_USB_VENDOR : OSK_LINK_USB_HID;
		(void)gh_call(msg, len, link, &reply);
		if (reply.len > 0u) {
			answers++;
		}
		if (!reply_is_sane(&reply)) {
			printf("FEHLER unlesbare Antwort nach %lu Runden (%zu Byte)\n", rounds,
			       reply.len);
			bad = 1;
			break;
		}
		rounds++;
	}

	printf("fuzz_frame: %lu Runden, %lu Antworten, %d Fehler\n", rounds, answers, bad);
	if (rounds == 0u) {
		printf("ABBRUCH: keine Runde gelaufen\n");
		return 2;
	}
	if (bad == 0) {
		printf("fuzzlauf: ok\n");
	}
	return bad;
}
