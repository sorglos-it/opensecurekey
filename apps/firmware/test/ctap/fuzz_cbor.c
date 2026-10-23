/* Fuzzlauf ueber den CBOR-Parser und die CTAP-Befehle.
 *
 * Genau hier kommt ein Angreifer zuerst an: er schickt Bytes, und der
 * Parser muss sie abweisen, ohne ueber einen Puffer zu laufen. Der Lauf
 * wirft erst Zufall hinein, dann verbogene Fassungen echter Nachrichten -
 * reiner Zufall kommt an den Pflichtfeldern nicht weit genug vorbei.
 *
 * Aufruf: fuzz_cbor [Sekunden] [Startwert]
 * Mit -fsanitize=address,undefined uebersetzt (siehe Makefile) wird jeder
 * Ueberlauf zum Abbruch, nicht zu einem stillen Fehler.
 *
 * Der Lauf prueft zwei Zusicherungen:
 *   1. Kein Aufruf darf den Prozess umbringen oder ueber Puffer schreiben.
 *   2. Antwortet der Kern mit CTAP2_OK, dann ist die Antwort kanonisches
 *      CBOR - der Schreiber darf nie etwas ausgeben, was unser eigener
 *      strenger Leser ablehnt. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mocks.h"
#include "osk_cbor.h"
#include "osk_ctap.h"
#include "osk_ctap_internal.h"
#include "osk_crypto.h"
#include "osk_transport.h"

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

/* Eine echte, vollstaendige MakeCredential-Nachricht als Vorlage. */
static size_t sample_make_credential(uint8_t *out, size_t cap)
{
	osk_cbor_writer_t w;
	uint8_t hash[32];

	osk_sha256((const uint8_t *)"clientData", 10, hash);
	out[0] = OSK_CTAP_CMD_MAKE_CREDENTIAL;
	osk_cbor_writer_init(&w, out + 1, cap - 1);
	osk_cbor_put_map(&w, 6);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, (const uint8_t *)"anna", 4);
	osk_cbor_put_text(&w, "name");
	osk_cbor_put_text(&w, "anna");
	osk_cbor_put_uint(&w, 4);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "alg");
	osk_cbor_put_int(&w, -7);
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	osk_cbor_put_uint(&w, 6);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "credProtect");
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_text(&w, "hmac-secret");
	osk_cbor_put_bool(&w, true);
	osk_cbor_put_uint(&w, 7);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "rk");
	osk_cbor_put_bool(&w, true);
	return 1 + w.len;
}

static size_t sample_get_assertion(uint8_t *out, size_t cap)
{
	osk_cbor_writer_t w;
	uint8_t hash[32];
	uint8_t id[97];

	osk_sha256((const uint8_t *)"clientData", 10, hash);
	for (unsigned i = 0; i < sizeof(id); i++)
		id[i] = (uint8_t)i;

	out[0] = OSK_CTAP_CMD_GET_ASSERTION;
	osk_cbor_writer_init(&w, out + 1, cap - 1);
	osk_cbor_put_map(&w, 4);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_text(&w, "example.com");
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_bytes(&w, hash, 32);
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_array(&w, 1);
	osk_cbor_put_map(&w, 2);
	osk_cbor_put_text(&w, "id");
	osk_cbor_put_bytes(&w, id, sizeof(id));
	osk_cbor_put_text(&w, "type");
	osk_cbor_put_text(&w, "public-key");
	osk_cbor_put_uint(&w, 4);
	osk_cbor_put_map(&w, 1);
	osk_cbor_put_text(&w, "hmac-secret");
	osk_cbor_put_map(&w, 3);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_map(&w, 5);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_int(&w, -25);
	osk_cbor_put_int(&w, -1);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_int(&w, -2);
	osk_cbor_put_bytes(&w, id, 32);
	osk_cbor_put_int(&w, -3);
	osk_cbor_put_bytes(&w, id + 32, 32);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_bytes(&w, id, 48);
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_bytes(&w, id, 32);
	return 1 + w.len;
}

static size_t sample_client_pin(uint8_t *out, size_t cap)
{
	osk_cbor_writer_t w;
	uint8_t blob[64];

	for (unsigned i = 0; i < sizeof(blob); i++)
		blob[i] = (uint8_t)(i * 3u);

	out[0] = OSK_CTAP_CMD_CLIENT_PIN;
	osk_cbor_writer_init(&w, out + 1, cap - 1);
	osk_cbor_put_map(&w, 6);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_uint(&w, 9);
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_map(&w, 5);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_uint(&w, 2);
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_int(&w, -25);
	osk_cbor_put_int(&w, -1);
	osk_cbor_put_uint(&w, 1);
	osk_cbor_put_int(&w, -2);
	osk_cbor_put_bytes(&w, blob, 32);
	osk_cbor_put_int(&w, -3);
	osk_cbor_put_bytes(&w, blob + 32, 32);
	osk_cbor_put_uint(&w, 6);
	osk_cbor_put_bytes(&w, blob, 48);
	osk_cbor_put_uint(&w, 9);
	osk_cbor_put_uint(&w, 3);
	osk_cbor_put_uint(&w, 10);
	osk_cbor_put_text(&w, "example.com");
	return 1 + w.len;
}

/* Verbiegt eine Vorlage: Bytes tauschen, abschneiden, anhaengen. */
static size_t mutate(const uint8_t *in, size_t len, uint8_t *out, size_t cap)
{
	size_t n = len;

	if (n > cap)
		n = cap;
	memcpy(out, in, n);

	const unsigned rounds = 1u + (rnd() % 6u);
	for (unsigned i = 0; i < rounds; i++) {
		switch (rnd() % 5u) {
		case 0: /* ein Byte ersetzen */
			if (n)
				out[rnd_below(n)] = (uint8_t)rnd();
			break;
		case 1: /* ein Bit kippen */
			if (n)
				out[rnd_below(n)] ^= (uint8_t)(1u << (rnd() % 8u));
			break;
		case 2: /* abschneiden */
			if (n > 1)
				n = 1 + rnd_below(n - 1);
			break;
		case 3: /* anhaengen */
			if (n < cap)
				out[n++] = (uint8_t)rnd();
			break;
		default: /* eine Laengenangabe gross machen */
			if (n)
				out[rnd_below(n)] |= 0x1f;
			break;
		}
	}
	return n;
}

int main(int argc, char **argv)
{
	const int secs = (argc > 1) ? atoi(argv[1]) : 10;
	g_rng = (argc > 2) ? (uint64_t)strtoull(argv[2], NULL, 0) : 0x12345678abcdefULL;
	if (g_rng == 0)
		g_rng = 1;

	uint8_t templ[3][1024];
	size_t templ_len[3];
	uint8_t msg[1400];
	uint8_t out[2048];

	mock_reset_all();
	if (osk_ctap_init(OSK_CTAP_EXT_HMAC_SECRET | OSK_CTAP_EXT_CRED_PROTECT) != OSK_OK) {
		printf("FEHLER: Kern startet nicht\n");
		return 1;
	}
	/* Die Anzeige sagt immer ja: so kommt der Fuzzer bis in die Tiefe. */
	mock_ui_set_confirm(OSK_OK);

	templ_len[0] = sample_make_credential(templ[0], sizeof(templ[0]));
	templ_len[1] = sample_get_assertion(templ[1], sizeof(templ[1]));
	templ_len[2] = sample_client_pin(templ[2], sizeof(templ[2]));

	unsigned long runs = 0, ok_answers = 0, pure_random = 0;
	unsigned long accepted_random = 0;
	const time_t start = time(NULL);

	while (time(NULL) - start < secs) {
		size_t len;

		if ((rnd() % 4u) == 0u) {
			/* reiner Zufall */
			len = 1 + rnd_below(200);
			for (size_t i = 0; i < len; i++)
				msg[i] = (uint8_t)rnd();
			pure_random++;
		} else {
			const unsigned which = rnd() % 3u;
			len = mutate(templ[which], templ_len[which], msg, sizeof(msg));
		}

		osk_request_t req;
		osk_response_t res;
		memset(&req, 0, sizeof(req));
		memset(&res, 0, sizeof(res));
		req.link = (rnd() % 8u == 0u) ? OSK_LINK_NFC : OSK_LINK_USB_HID;
		req.data = msg;
		req.len = len;
		res.data = out;
		res.cap = sizeof(out);

		/* Reset wuerde das Grundgeheimnis wegwerfen; das gehoert nicht
		 * in diesen Lauf, es macht ihn nur langsam. */
		if (len > 0 && msg[0] == OSK_CTAP_CMD_RESET)
			msg[0] = OSK_CTAP_CMD_GET_INFO;

		if (osk_ctap_handle(&req, &res) == OSK_OK && res.len >= 1) {
			if (out[0] == CTAP2_OK) {
				ok_answers++;
				if (pure_random && res.len > 1)
					accepted_random++;
				/* Zusicherung: eine erfolgreiche Antwort ist
				 * immer kanonisches CBOR. */
				if (res.len > 1 &&
				    osk_cbor_validate(out + 1, res.len - 1) != OSK_OK) {
					printf("FEHLER: Antwort ist kein kanonisches CBOR\n");
					printf("  Eingabe (%zu Bytes), Startwert anders setzen "
					       "zum Nachstellen\n",
					       len);
					return 1;
				}
			}
		}

		/* Und der reine CBOR-Pruefer gegen dieselben Bytes. */
		(void)osk_cbor_validate(msg, len);
		runs++;
	}

	printf("fuzz_cbor: %lu Durchgaenge, davon %lu reiner Zufall, "
	       "%lu erfolgreiche Antworten\n",
	       runs, pure_random, ok_answers);
	if (runs == 0) {
		printf("FEHLER: kein einziger Durchgang\n");
		return 1;
	}
	/* Ohne erfolgreiche Antwort haette der Lauf nur die aeussere Wand
	 * geprueft und waere wertlos. */
	if (ok_answers == 0) {
		printf("FEHLER: keine einzige Nachricht kam durch - der Lauf "
		       "hat nur die erste Pruefung getroffen\n");
		return 1;
	}
	printf("fuzzlauf: ok\n");
	return 0;
}
